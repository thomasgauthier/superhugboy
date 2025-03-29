-- Zelda "Take This" Challenge Handler


return {
    {
        game_slug = "zelda1",
        rom_path = "game_data/ROMS/Legend of Zelda, The (USA) (Rev 1).zip",
        savestate_path =  "game_data/states/Legend of Zelda - take this.State",
        challenge_text = "Act out the legend!",
        challenge_text_pos = { x = 100, y = 100 },
        weight = 1,
        handler = function(state, reset)
            -- Address to check for the "take this" event
            local check_addr = 0x0657
            local current_value = memory.readbyte(check_addr)


            
            local room_addr = 0x006
            local room_value = memory.readbyte(room_addr)

            
            -- Initialize prev_value if it doesn't exist
            if state.prev_value == nil then
                state.prev_value = current_value
            end

            if state.prev_room_value == nil then
                state.prev_room_value = room_value
            end
        
            -- Check for transition from 0 to non-zero
            if state.prev_value == 0 and current_value ~= 0 then
                -- Return the number of seconds to switch
                return 1.0 -- Switch after 1 second
            end

            -- print(state.prev_room_value, room_value)
            if state.prev_room_value == 0 and room_value ~= 0 then
                reset()
            end

            
        
            -- Store current value for next comparison
            state.prev_value = current_value

            state.prev_room_value = room_value
            
            -- Return nil to continue the challenge
            return nil
        end
    },
    {
        game_slug = "zelda1",
        rom_path = "game_data/ROMS/Legend of Zelda, The (USA) (Rev 1).zip",
        savestate_path = "game_data/states/Legend of Zelda - boss 1.State",
        challenge_text = "Defeat the boss!",
        challenge_text_pos = { x = 100, y = 100 },
        weight = 1,
        handler = function(state, reset)
            local health_addr = 0x066F
            local sub_hp_addr = 0x0670
            local room_addr = 0x00EB
            
            local byte_value = memory.readbyte(health_addr)
            local heart_container_count = (byte_value & 0xF0) >> 4
            local filled_hearts = byte_value & 0x0F
            local sub_hp = memory.readbyte(sub_hp_addr)
            local current_room_value = memory.readbyte(room_addr)

            state.prev_heart_container_count = state.prev_heart_container_count or heart_container_count
            state.prev_filled_hearts = state.prev_filled_hearts or filled_hearts
            state.prev_sub_hp = state.prev_sub_hp or sub_hp
            state.prev_room_value = state.prev_room_value or current_room_value

            if (state.prev_filled_hearts > 0 or state.prev_sub_hp > 0) and filled_hearts == 0 and sub_hp == 0 then
                -- Modified to return time value instead of scheduling
                return 0.8 -- Switch after 0.8 seconds (roughly 48 frames at 60fps)
            end

            if heart_container_count > state.prev_heart_container_count then
                -- Return to switch immediately on heart container increase
                return 0.5 -- Switch after ~1 frame
            end

            if state.prev_room_value ~= 69 and current_room_value == 69 then
                -- Reset the challenge when entering room 69
                reset()
                -- Don't return a value here to continue the challenge
            end

            state.prev_heart_container_count = heart_container_count
            state.prev_filled_hearts = filled_hearts
            state.prev_sub_hp = sub_hp
            state.prev_room_value = current_room_value
            
            -- Return nil to continue the challenge
            return nil
        end
    }
} 