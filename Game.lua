local DEBUG = false

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


local challenge_text_timer = 5


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
    require("./challenges/LinksAwakening"),
    require("./challenges/StreetFighter"),
    require("./challenges/Metroid"),
    require("./challenges/Metroid Classic"),
    require("./challenges/Pokemon"),
    require("./challenges/Sonic"),
    require("./challenges/StreetsofRage2"),
    require("./challenges/ALinkToThePast"),
    require("./challenges/Kirby"),
    require("./challenges/DonkeyKongCountry"),
    require("./challenges/Tetris"),
    require("./challenges/Castlevania"),
    require("./challenges/Zelda1"),
    require("./challenges/Megaman"),
    require("./challenges/Mario1"),
    require("./challenges/Mario3"),
    require("./challenges/Earthbound"),
    require("./challenges/Starfox"),
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
local current_rom_path = nil  -- Track the current ROM path

-- Dynamic weight system parameters
local dynamic_weights = {}
local base_weight_multiplier = 1.0  -- Base weight multiplier
local played_penalty = 0.05         -- Weight after being played (5% of original)
local recovery_rate = 0.012         -- Weight recovery per second (2% per second)
local DateTime = luanet.System.DateTime
local last_weight_update = DateTime.UtcNow -- Track last weight update time
local frames_since_last_change = 0  -- Track frames for weight recovery

-- Initialize dynamic weights based on challenge handler weights
local function init_dynamic_weights()
    for i, handler in ipairs(challenge_handlers) do
        dynamic_weights[i] = handler.weight or 1.0  -- Use handler weight or default to 1
    end
end

-- Select next challenge based on dynamic weights
local function select_weighted_challenge()
    -- Calculate total weight
    local total_weight = 0
    for i, weight in ipairs(dynamic_weights) do
        -- Skip the interlude when calculating weights
        if i ~= "interlude" then
            total_weight = total_weight + weight
        end
    end
    
    -- Select a random value within the total weight
    local selection = math.random() * total_weight
    
    -- Find which challenge was selected
    local cumulative_weight = 0
    for i, weight in ipairs(dynamic_weights) do
        -- Skip the interlude when selecting
        if i ~= "interlude" then
            cumulative_weight = cumulative_weight + weight
            if selection <= cumulative_weight then
                return i
            end
        end
    end
    
    -- Fallback (should rarely happen due to floating-point precision)
    -- Make sure to exclude interlude from random selection
    local valid_indices = {}
    for i = 1, #challenge_handlers do
        if i ~= "interlude" then
            table.insert(valid_indices, i)
        end
    end
    return valid_indices[math.random(#valid_indices)]
end

-- Update the weights of all challenges
local function update_weights()
    -- Increase weights of all challenges gradually
    for i = 1, #dynamic_weights do
        local original_weight = challenge_handlers[i].weight or 1.0
        local max_weight = original_weight * base_weight_multiplier
        
        -- Only increase weight if it's below the max
        if dynamic_weights[i] < max_weight then
            dynamic_weights[i] = math.min(
                max_weight,
                dynamic_weights[i] + (original_weight * recovery_rate)
            )
        end
    end
end

-- Reduce weight after a challenge is played
local function reduce_weight(challenge_index)
    local original_weight = challenge_handlers[challenge_index].weight or 1.0
    dynamic_weights[challenge_index] = original_weight * played_penalty
end

-- Print the current weights (for debugging)
local function print_weights()
    print("Current dynamic weights:")
    for i, weight in ipairs(dynamic_weights) do
        local handler = challenge_handlers[i]
        print(string.format("%s: %.2f", handler.game_slug, weight))
    end
end

-- Challenge switch timer
local switch_timer = {
    active = false,
    frames_left = 0
}

-- Interlude timer
local interlude_timer = {
    last_interlude = DateTime.UtcNow,
    interval = 180  -- 5 seconds
}

challenge_handlers['interlude'] = require("./challenges/MarioWorldInterlude")[1]

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
    -- Check if it's time for an interlude
    local current_time = DateTime.UtcNow

    if current_challenge == "interlude" then
        interlude_timer.last_interlude = current_time
    end

    
    -- Reduce weight of current challenge
    reduce_weight(current_challenge)


    local next_challenge = nil

    local current_time = DateTime.UtcNow
    local time_diff = (current_time - interlude_timer.last_interlude).TotalSeconds
    if time_diff >= interlude_timer.interval then
        print("interlude")
        next_challenge = "interlude"
    else
        next_challenge = select_weighted_challenge()
    end
    
    -- Select next challenge based on weights
    
    -- Avoid playing the same challenge twice in a row if possible
    if next_challenge == current_challenge and #challenge_handlers > 1 then
        -- Try up to 3 times to get a different challenge
        for attempt = 1, 3 do
            local candidate = select_weighted_challenge()
            if candidate ~= current_challenge then
                next_challenge = candidate
                break
            end
        end
    end
    
    current_challenge = next_challenge
    state = {}  -- Reset state for the new challenge
    state.text_display_timer = seconds_to_frames(challenge_text_timer)
    
    -- Reset frames counter for weight recovery
    frames_since_last_change = 0
    
    -- Load the new challenge
    local challenge = challenge_handlers[current_challenge]
    if challenge then
        print("Switching to challenge: " .. challenge.game_slug)
        
        -- Only reload ROM if it's different from the current one
        if current_rom_path ~= challenge.rom_path then
            client.openrom(challenge.rom_path)
            current_rom_path = challenge.rom_path
        else
            print("Same ROM detected, skipping reload")
        end
        
        savestate.load(challenge.savestate_path)
    end
end

-- Load the first challenge
if #challenge_handlers > 0 then
    -- Initialize random seed
    math.randomseed(os.time())
    
    -- Initialize dynamic weights
    init_dynamic_weights()
    
    -- Select the first challenge
    current_challenge = select_weighted_challenge()
    
    local challenge = challenge_handlers[current_challenge]
    if challenge then
        print("Loading challenge: " .. challenge.game_slug)
        -- print_weights()  -- Print initial weights
        
        -- Always load the ROM for the first challenge
        client.openrom(challenge.rom_path)
        current_rom_path = challenge.rom_path
        
        savestate.load(challenge.savestate_path)
    end
end

local prev_t_state = false

while true do
    -- Update frames counter for weight recovery
    frames_since_last_change = frames_since_last_change + 1
    
    -- Update weights every second using DateTime
    local current_time = DateTime.UtcNow
    local time_diff = (current_time - last_weight_update).TotalSeconds
    if time_diff >= 1 then
        update_weights()
        last_weight_update = current_time
    end
    
    -- Display time until next interlude
    if DEBUG then
        local time_until_interlude = interlude_timer.interval - (current_time - interlude_timer.last_interlude).TotalSeconds
        gui.drawText(10, 50, string.format("Time until interlude: %.1f seconds", time_until_interlude), "white", "black")
    end
    
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
    
    -- Initialize reset timer
    state.reset_timer = state.reset_timer or { active = false, frames_left = 0 }
    
    -- Check if reset timer is active
    if state.reset_timer.active then
        state.reset_timer.frames_left = state.reset_timer.frames_left - 1
        if state.reset_timer.frames_left <= 0 then
            state.reset_timer.active = false
            state = {} -- reset state
            savestate.load(challenge_handlers[current_challenge].savestate_path) -- reload save state
        end
    end
    
    -- Execute the handler with state
    if handler then
        local seconds_to_switch = handler(state, function(seconds)
            if seconds and type(seconds) == "number" then
                -- Set up delayed reset
                state.reset_timer.active = true
                state.reset_timer.frames_left = seconds_to_frames(seconds)
            else
                -- Immediate reset
                state = {
                    text_display_timer = seconds_to_frames(challenge_text_timer)
                }

                savestate.load(challenge_handlers[current_challenge].savestate_path) -- reload save state
            end
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

            if state.text_display_timer == nil then
                state.text_display_timer = seconds_to_frames(challenge_text_timer)
            end

            
            -- Initialize text display timer if not already set
            -- Only show text if timer hasn't expired
            if state.text_display_timer > 0 then
                state.text_display_timer = state.text_display_timer - 1

                if state.text_display_timer <= 0 then
                    print("clearing text")
                    gui.drawText(0, 0, "", "white", "white") -- keep this, need to clear text

                    gui.cleartext()
                else
                    gui.drawText(x_pos, y_pos, challenge.challenge_text, "yellow", "black", 14, nil, "bold", "center")
                end
            end
            
            -- Display switch timer if active
            if switch_timer.active then
                if DEBUG then
                    local fps = get_core_fps()
                    local seconds_left = math.ceil(switch_timer.frames_left / fps)
                    gui.drawText(10, 30, "Next challenge in: " .. seconds_left .. "s", "white", "black")
                end
            end
        end

        gui.cleartext()

    else
        gui.drawText(10, 10, "No handler found.", "red")
    end
    
    emu.frameadvance()
end