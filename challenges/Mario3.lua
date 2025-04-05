-- Mario Boss Challenge Handler

local game_slug = "mario3"
local rom_path = "game_data/ROMS/Super Mario Bros. 3 (USA) (Rev 1).zip"

function check_death_and_switch(state)
    local fanfare_play_byte = 0x04F4
    local death_fanfare_value = 1
    local current_fanfare = memory.readbyte(fanfare_play_byte)

    if state.prev_fanfare == nil then
        state.prev_fanfare = current_fanfare
    end

    if state.prev_fanfare ~= death_fanfare_value and current_fanfare == death_fanfare_value then
        state.prev_fanfare = current_fanfare
        return true
    end
    state.prev_fanfare = current_fanfare
    return false
end


return {
    {
        game_slug = game_slug,
        rom_path = rom_path,
        savestate_path = "game_data/states/Super Mario Bros. 3 - first mini boss.State",
        challenge_text = "Beat the boss!",
        challenge_text_pos = { x = 128, y = 96 },
        weight = 0.5,
        handler = function(state)
            local trigger_addr = 0x05F3
            local trigger_value = memory.readbyte(trigger_addr)
        
            if state.prev_trigger_value == nil then
                state.prev_trigger_value = trigger_value
            end
        
            if state.prev_trigger_value ~= 0x01 and trigger_value == 0x01 then
                return 0.75
            end

            if check_death_and_switch(state) then
                return 0.75
            end
        
            state.prev_trigger_value = trigger_value
        end
    },
    {
        game_slug = game_slug,
        rom_path = rom_path,
        savestate_path = "game_data/states/Super Mario Bros. 3 - crushing ceiling.State",
        challenge_text = "Reach the door!",
        challenge_text_pos = { x = 128, y = 96 },
        weight = 0.5,
        handler = function(state)
            local check_addr = 0x0075
            local current_value = memory.readbyte(check_addr)
            state.prev_value = state.prev_value or current_value

            if state.prev_value ~= 7 and current_value == 7 then
                return 0.016 -- Switch immediately (~1 frame)
            end

            if check_death_and_switch(state) then
                return 0.8 -- Switch after 0.8 seconds (~48 frames at 60fps)
            end
            
            state.prev_value = current_value
            
            -- Return nil to continue the challenge
            return nil
        end
    },
    {
        game_slug = game_slug,
        rom_path = rom_path,
        savestate_path = "game_data/states/Super Mario Bros. 3 - hammer bro.State",
        challenge_text = "Beat the hammer bro!",
        challenge_text_pos = { x = 128, y = 72 },
        weight = 0.5,
        handler = function(state)
            local check_addr = 0x0075
            local current_value = memory.readbyte(check_addr)
            local object_id = memory.readbyte(0x05F3)
            state.prev_value = state.prev_value or current_value

            if object_id == 2 then
                return 1.6
            end

            if state.prev_value ~= 7 and current_value == 7 then
                return 0.016 -- Switch immediately (~1 frame)
            end

            if check_death_and_switch(state) then
                return 0.8 -- Switch after 0.8 seconds (~48 frames at 60fps)
            end
            
            state.prev_value = current_value
            
            -- Return nil to continue the challenge
            return nil
        end
    }
}