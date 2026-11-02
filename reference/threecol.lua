-- Three-column layout for the ultrawide:
--   [ left: 1 window ] [ middle: master ] [ right: everything else, stacked ]
-- Enable with general.layout = "lua:threecol" (core/general.lua).
--
-- Each window remembers its column. New windows fill the middle first, then the
-- left, then go to the bottom of the right stack. Closing a window leaves its
-- spot empty (nothing slides over); the next new window fills it.
--
-- layoutmsg "move l|r|u|d" (SUPER+SHIFT+HJKL) moves/swaps windows,
-- layoutmsg "focus l|r|u|d" (SUPER+HJKL) moves focus, both in the same order:
--   l/r walk  left <-> middle <-> right top <-> ... <-> right bottom
--   u/d go up/down inside the right stack

local SIDE = 0.25 -- width of each side column (middle gets the rest)

-- Per-window state, keyed by stable_id (addresses get reused after a window closes):
--   col[k]  = "l" | "m" | "r"
--   rank[k] = order (used for opening order and position in the right stack)
local col, rank, nextRank = {}, {}, 0

local function key(t)
	return t.window and t.window.stable_id or ("target" .. t.index)
end

-- Sort the workspace's windows into { left = t|nil, mid = t|nil, right = {t...} }.
local function columns(ctx)
	local list = {}
	for _, t in ipairs(ctx.targets) do
		local k = key(t)
		if not rank[k] then
			nextRank = nextRank + 1
			rank[k] = nextRank
		end
		list[#list + 1] = t
	end
	table.sort(list, function(x, y)
		return rank[key(x)] < rank[key(y)]
	end)

	local view = { right = {} }
	local fresh = {}
	-- windows that already have a column keep it (extras on a taken side spot go right)
	for _, t in ipairs(list) do
		local c = col[key(t)]
		if c == "m" and not view.mid then
			view.mid = t
		elseif c == "l" and not view.left then
			view.left = t
		elseif c then
			col[key(t)] = "r"
			table.insert(view.right, t)
		else
			fresh[#fresh + 1] = t
		end
	end
	-- new windows: middle, then left, then bottom of the right stack
	for _, t in ipairs(fresh) do
		local k = key(t)
		if not view.mid then
			view.mid, col[k] = t, "m"
		elseif not view.left then
			view.left, col[k] = t, "l"
		else
			col[k] = "r"
			table.insert(view.right, t)
		end
	end
	return view
end

local function place(ctx, v)
	local a = ctx.area
	local sideW = a.w * SIDE

	if v.mid then
		v.mid:place({ x = a.x + sideW, y = a.y, w = a.w - 2 * sideW, h = a.h })
	end
	if v.left then
		v.left:place({ x = a.x, y = a.y, w = sideW, h = a.h })
	end
	local n = #v.right
	for i, t in ipairs(v.right) do
		local rowH = a.h / n
		t:place({ x = a.x + a.w - sideW, y = a.y + (i - 1) * rowH, w = sideW, h = rowH })
	end
end

-- Cells in left-to-right order. Each is { col = "l"|"m"|"r", t = target|nil }.
-- An empty right column still gets one empty cell so you can move a window there.
local function cells(v)
	local c = { { col = "l", t = v.left }, { col = "m", t = v.mid } }
	if #v.right == 0 then
		c[3] = { col = "r" }
	end
	for _, t in ipairs(v.right) do
		c[#c + 1] = { col = "r", t = t }
	end
	return c
end

-- Drag & drop (SUPER + left drag): during a drag Hyprland floats the window and
-- puts it back on release. The SUPER+click bind arms detection (core/keybinds.lua
-- -> arm_drag) so a normal float toggle isn't mistaken for a drag. When the
-- dragged window turns tiled again, it swaps with whatever is under the cursor
-- (or moves into an empty column).
local dragArmedAt
local dragging = {} -- stable_id -> true while being dragged
local pendingDrop -- dropped window not back in the layout yet

local M = {}
function M.arm_drag()
	dragArmedAt = os.time()
end

hl.on("window.update_state", function(w, s)
	if not w or s == nil or s.float == nil then
		return
	end
	local id = w.stable_id
	if s.float then
		if dragArmedAt and os.time() - dragArmedAt <= 2 then
			dragging[id] = true
			dragArmedAt = nil
		end
	elseif dragging[id] then
		dragging[id] = nil
		hl.dispatch(hl.dsp.layout("drop " .. id))
	end
end)

local function has(ctx, id)
	for _, t in ipairs(ctx.targets) do
		if key(t) == id then
			return true
		end
	end
	return false
end

local function drop(ctx, id)
	local cur = hl.get_cursor_pos()
	if not cur then
		return
	end
	local v = columns(ctx)
	local a = ctx.area
	local sideW = a.w * SIDE

	-- the right stack as it looked during the drag (without the dragged window)
	local right = {}
	for _, t in ipairs(v.right) do
		if key(t) ~= id then
			right[#right + 1] = t
		end
	end

	local toCol, toT
	if cur.x < a.x + sideW then
		toCol, toT = "l", v.left
	elseif cur.x < a.x + a.w - sideW then
		toCol, toT = "m", v.mid
	else
		toCol = "r"
		if #right > 0 then
			local i = math.floor((cur.y - a.y) / (a.h / #right)) + 1
			toT = right[math.max(1, math.min(#right, i))]
		end
	end

	if toT then
		local b = key(toT)
		if b ~= id then
			col[id], col[b] = col[b], col[id]
			rank[id], rank[b] = rank[b], rank[id]
		end
	elseif col[id] ~= toCol then
		col[id] = toCol
		if toCol == "r" then
			nextRank = nextRank + 1
			rank[id] = nextRank
		end
	end
end

hl.layout.register("threecol", {
	recalculate = function(ctx)
		if pendingDrop and has(ctx, pendingDrop) then
			drop(ctx, pendingDrop)
			pendingDrop = nil
		end
		place(ctx, columns(ctx))
	end,

	layout_msg = function(ctx, msg)
		local active = hl.get_active_window()

		-- sent by the drag handler above when a dragged window is released
		local dropId = msg:match("^drop%s+(%S+)")
		if dropId then
			if has(ctx, dropId) then
				drop(ctx, dropId)
				place(ctx, columns(ctx))
			else
				pendingDrop = dropId
			end
			return true
		end

		-- SUPER+M: focus the middle window
		if msg:match("^focusmaster") then
			local mid = columns(ctx).mid
			if mid then
				hl.dispatch(hl.dsp.focus({ window = "address:" .. mid.window.address }))
			end
			return true
		end

		-- SUPER+SHIFT+M: swap the focused window with the middle one (focus stays on it)
		if msg:match("^swapwithmaster") then
			if not active then
				return true
			end
			local v = columns(ctx)
			local a = active.stable_id
			if not col[a] or active.floating then
				return true -- not a tiled window of this layout
			end
			if v.mid and key(v.mid) == a then
				return true -- already the master
			end
			if v.mid then
				local b = key(v.mid)
				col[a], col[b] = col[b], col[a]
				rank[a], rank[b] = rank[b], rank[a]
			else
				col[a] = "m"
			end
			place(ctx, columns(ctx))
			return true
		end

		local cmd, dir = msg:match("^(%a+)%s+([lrud])")
		if cmd ~= "move" and cmd ~= "focus" then
			return false
		end

		if not active then
			return true
		end

		local v = columns(ctx)
		local c = cells(v)
		local pos
		for i, cell in ipairs(c) do
			if cell.t and key(cell.t) == active.stable_id then
				pos = i
			end
		end
		if not pos then
			return true
		end

		-- u/d only make sense inside the right stack
		local step = (dir == "r" or dir == "d") and 1 or -1
		if dir == "u" or dir == "d" then
			if c[pos].col ~= "r" or not (c[pos + step] and c[pos + step].col == "r") then
				return true
			end
		end

		if cmd == "focus" then
			-- skip empty spots
			local i = pos + step
			while c[i] and not c[i].t do
				i = i + step
			end
			if c[i] then
				hl.dispatch(hl.dsp.focus({ window = "address:" .. c[i].t.window.address }))
			end
			return true
		end

		local from, to = c[pos], c[pos + step]
		if not to then
			return true
		end
		local a = key(from.t)
		if to.t then
			-- swap columns and stack positions
			local b = key(to.t)
			col[a], col[b] = col[b], col[a]
			rank[a], rank[b] = rank[b], rank[a]
		else
			-- move into the empty spot
			col[a] = to.col
		end
		place(ctx, columns(ctx))
		return true
	end,
})

return M
