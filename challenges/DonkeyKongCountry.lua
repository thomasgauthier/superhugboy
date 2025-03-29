-- Donkey Kong Country Challenge Handler

return {
    {
        game_slug = "donkeykong",
        rom_path = "game_data/ROMS/Donkey Kong Country (USA) (Rev 2).zip",
        savestate_path = "game_data/states/Donkey Kong Country - level 1.State",
        challenge_text = "Finish the level!",
        challenge_text_pos = { x = 68, y = 96 },
        weight = 1,
        handler = function(state, reset)
            local game_state_addr = 0x0040
            local lives_addr = 0x0575
            local game_state = memory.readbyte(game_state_addr)
            local lives = memory.readbyte(lives_addr)

            state.prev_game_state = state.prev_game_state or game_state
            state.prev_lives = state.prev_lives or lives

            if state.prev_game_state ~= 12 and game_state == 12 then
                -- Return to switch immediately on level completion
                return 0.016 -- Switch after ~1 frame
            end

            if lives < state.prev_lives then
                -- Return to switch after losing a life
                return 0.8 -- Switch after 0.8 seconds (~48 frames at 60fps)
            end

            state.prev_game_state = game_state
            state.prev_lives = lives
            
            -- Return nil to continue the challenge
            return nil
        end
    },
    {
        game_slug = "donkeykong",
        rom_path = "game_data/ROMS/Donkey Kong Country (USA) (Rev 2).zip",
        savestate_path = "game_data/states/Donkey Kong Country - barrel level.State",
        challenge_text = "Pass the barrels!",
        challenge_text_pos = { x = 68, y = 96 },
        weight = 1,
        handler = function(state, reset)
            local game_state_addr = 0x0040
            local lives_addr = 0x0575
            local game_state = memory.readbyte(game_state_addr)
            local lives = memory.readbyte(lives_addr)
            local x_position = mainmemory.read_u16_le(0x0000BE)

            state.prev_game_state = state.prev_game_state or game_state
            state.prev_lives = state.prev_lives or lives

            if x_position > 4800 then
                return 0.5
            end

            if state.prev_game_state ~= 12 and game_state == 12 then
                -- Return to switch immediately on level completion
                return 0.016 -- Switch after ~1 frame
            end

            if lives < state.prev_lives then
                -- Return to switch after losing a life
                return 0.8 -- Switch after 0.8 seconds (~48 frames at 60fps)
            end

            state.prev_game_state = game_state
            state.prev_lives = lives
            
            -- Return nil to continue the challenge
            return nil
        end
    },
    {
        game_slug = "donkeykong",
        rom_path = "game_data/ROMS/Donkey Kong Country (USA) (Rev 2).zip",
        savestate_path = "game_data/states/Donkey Kong Country - boss 1.State",
        challenge_text = "Beat the boss!",
        challenge_text_pos = { x = 74, y = 96 },
        weight = 1,
        handler = function(state, reset)
            local game_state_addr = 0x0040
            local lives_addr = 0x0575
            local game_state = memory.readbyte(game_state_addr)
            local lives = memory.readbyte(lives_addr)
            local whatever_this_is = mainmemory.read_u16_le(0x000B20)

            state.prev_game_state = state.prev_game_state or game_state
            state.prev_lives = state.prev_lives or lives

            if whatever_this_is == 1 then
                return 0.8
            end

            if state.prev_game_state ~= 12 and game_state == 12 then
                -- Return to switch immediately on level completion
                return 0.016 -- Switch after ~1 frame
            end

            if lives < state.prev_lives then
                -- Return to switch after losing a life
                return 0.8 -- Switch after 0.8 seconds (~48 frames at 60fps)
            end

            state.prev_game_state = game_state
            state.prev_lives = lives
            
            -- Return nil to continue the challenge
            return nil
        end
    }
}
