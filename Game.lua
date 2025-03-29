function reloadAll()
    -- Store the currently loaded module names
    local loaded = {}
    for moduleName in pairs(package.loaded) do
        loaded[moduleName] = true
    end

    -- Clear all loaded modules
    for moduleName in pairs(loaded) do
        package.loaded[moduleName] = nil
    end

    -- Clear JIT cache if using LuaJIT
    if jit and jit.flush then
        jit.flush()
    end

    -- Reload all previously loaded modules
    for moduleName in pairs(loaded) do
        -- Skip built-in modules to avoid potential issues
        if not _G[moduleName] then
            pcall(require, moduleName)
        end
    end
end
reloadAll()


-- Challenge handler shape for reference:
--[[
{
    game_slug = string,        -- Unique identifier for the game
    rom_path = string,         -- Path to the ROM file
    savestate_path = string,   -- Path to the savestate
    challenge_text = string,   -- Text displayed during challenge
    challenge_text_pos = table, -- Position for the challenge text
    weight = number,           -- Weight for random selection (default: 1)
    handler = function         -- Challenge logic function
}
]]

local challenge_modules_repository = {
    -- require("./challenges/LinksAwakening"),
    -- require("./challenges/StreetFighter"),
    -- require("./challenges/Metroid"),
    -- require("./challenges/Pokemon"),
    -- require("./challenges/Sonic"),
    -- require("./challenges/StreetsofRage2"),
    -- require("./challenges/Kirby"),
    require("./challenges/DonkeyKongCountry"),
    -- require("./challenges/Tetris"),
    -- require("./challenges/Castlevania"),
    -- require("./challenges/ALinkToThePast"),
    -- require("./challenges/Zelda1"),
    -- require("./challenges/Megaman"),
    -- require("./challenges/Mario1"),
    -- require("./challenges/Mario3"),
}


local challenge_handlers = {}

for _, v in ipairs(challenge_modules_repository) do
    for _, handler in ipairs(v) do
        table.insert(challenge_handlers, handler)
    end
end

-- Function to validate challenge handlers
local function validate_handler(handler, index)
    local errors = {}
    
    -- Check required fields
    if not handler.game_slug then
        table.insert(errors, "Missing required field: game_slug")
    elseif type(handler.game_slug) ~= "string" then
        table.insert(errors, "Invalid type for game_slug: expected string, got " .. type(handler.game_slug))
    end
    
    if not handler.rom_path then
        table.insert(errors, "Missing required field: rom_path")
    elseif type(handler.rom_path) ~= "string" then
        table.insert(errors, "Invalid type for rom_path: expected string, got " .. type(handler.rom_path))
    end
    
    if not handler.savestate_path then
        table.insert(errors, "Missing required field: savestate_path")
    elseif type(handler.savestate_path) ~= "string" then
        table.insert(errors, "Invalid type for savestate_path: expected string, got " .. type(handler.savestate_path))
    end
    
    if not handler.challenge_text then
        table.insert(errors, "Missing required field: challenge_text")
    elseif type(handler.challenge_text) ~= "string" then
        table.insert(errors, "Invalid type for challenge_text: expected string, got " .. type(handler.challenge_text))
    end
    
    if not handler.handler then
        table.insert(errors, "Missing required field: handler")
    elseif type(handler.handler) ~= "function" then
        table.insert(errors, "Invalid type for handler: expected function, got " .. type(handler.handler))
    end
    
    -- Optional fields validation
    if handler.challenge_text_pos ~= nil then
        if type(handler.challenge_text_pos) ~= "table" then
            table.insert(errors, "Invalid type for challenge_text_pos: expected table, got " .. type(handler.challenge_text_pos))
        else
            if handler.challenge_text_pos.x == nil then
                table.insert(errors, "Missing x coordinate in challenge_text_pos")
            elseif type(handler.challenge_text_pos.x) ~= "number" then
                table.insert(errors, "Invalid type for challenge_text_pos.x: expected number, got " .. type(handler.challenge_text_pos.x))
            end
            
            if handler.challenge_text_pos.y == nil then
                table.insert(errors, "Missing y coordinate in challenge_text_pos")
            elseif type(handler.challenge_text_pos.y) ~= "number" then
                table.insert(errors, "Invalid type for challenge_text_pos.y: expected number, got " .. type(handler.challenge_text_pos.y))
            end
        end
    end
    
    if handler.weight ~= nil and type(handler.weight) ~= "number" then
        table.insert(errors, "Invalid type for weight: expected number, got " .. type(handler.weight))
    end
    
    -- Report errors if any
    if #errors > 0 then
        print("Validation errors in challenge #" .. index .. " (" .. (handler.game_slug or "unknown") .. "):")
        for _, err in ipairs(errors) do
            print("  - " .. err)
        end
        return false
    end
    
    return true
end

-- Validate all challenge handlers
for i, handler in ipairs(challenge_handlers) do
    validate_handler(handler, i)
end

-- Initialize current challenge
local current_challenge = 1
local state = {}

-- Challenge switch timer
local switch_timer = {
    active = false,
    frames_left = 0
}

-- Get the current core's FPS
local function get_core_fps()
    return client.get_approx_framerate() or 60 -- Fallback to 60 if function not available
end

-- Convert seconds to frames based on the core's FPS
local function seconds_to_frames(seconds)
    local fps = get_core_fps()
    return math.floor(seconds * fps)
end

-- Set up a challenge switch after specified seconds
local function schedule_challenge_switch(seconds)
    if not switch_timer.active then
        switch_timer.active = true
        switch_timer.frames_left = seconds_to_frames(seconds)
        print("Challenge switch scheduled in " .. seconds .. " seconds (" .. switch_timer.frames_left .. " frames)")
    end
end

-- Switch to the next challenge
local function switch_to_next_challenge()
    local next_challenge = current_challenge + 1
    if next_challenge > #challenge_handlers then
        next_challenge = 1  -- Loop back to the first challenge
    end
    
    current_challenge = next_challenge
    state = {}  -- Reset state for the new challenge
    
    -- Load the new challenge
    local challenge = challenge_handlers[current_challenge]
    if challenge then
        print("Switching to challenge: " .. challenge.game_slug)
        client.openrom(challenge.rom_path)
        savestate.load(challenge.savestate_path)
    end
end

-- Load the first challenge
if #challenge_handlers > 0 then
    local challenge = challenge_handlers[current_challenge]
    if challenge then
        print("Loading challenge: " .. challenge.game_slug)
        client.openrom(challenge.rom_path)
        savestate.load(challenge.savestate_path)
    end
end

local prev_t_state = false

while true do
    -- Check if it's time to switch challenges
    if switch_timer.active then
        switch_timer.frames_left = switch_timer.frames_left - 1
        if switch_timer.frames_left <= 0 then
            switch_to_next_challenge()
            switch_timer.active = false
        end
    end
    
    -- Check for T key press to skip to next challenge
    local current_t_state = input.get()["T"]
    if current_t_state and not prev_t_state then
        switch_to_next_challenge()
        switch_timer.active = false
    end
    prev_t_state = current_t_state
    
    -- Get the current challenge handler
    local handler = challenge_handlers[current_challenge].handler
    
    -- Execute the handler with state
    if handler then
        local seconds_to_switch = handler(state, function()
            state = {} -- reset state
            savestate.load(challenge_handlers[current_challenge].savestate_path) -- reload save state
        end)
        
        -- If handler returns a number, schedule a challenge switch
        -- Only the first return value will schedule a switch, subsequent ones are ignored
        if seconds_to_switch and type(seconds_to_switch) == "number" then
            schedule_challenge_switch(seconds_to_switch)
        end
        
        -- Display challenge information
        local challenge = challenge_handlers[current_challenge]
        if challenge then
            local x_pos = 10
            local y_pos = 10
            
            if challenge.challenge_text_pos then
                x_pos = challenge.challenge_text_pos.x
                y_pos = challenge.challenge_text_pos.y
            end
            
            gui.drawText(x_pos, y_pos, challenge.challenge_text, "yellow", "black")
            
            -- Display switch timer if active
            if switch_timer.active then
                local fps = get_core_fps()
                local seconds_left = math.ceil(switch_timer.frames_left / fps)
                gui.drawText(10, 30, "Next challenge in: " .. seconds_left .. "s", "white", "black")
            end
        end
    else
        gui.drawText(10, 10, "No handler found.", "red")
    end
    
    emu.frameadvance()
end