-- A Link to the Past Cell Challenge Handler

return {
    {
        game_slug = "marioworld",
        rom_path = "game_data/ROMS/Super Mario World (USA).zip",
        savestate_path = "game_data/states/Super Mario World - level 1.State",
        challenge_text = "Beat the level!",
        challenge_text_pos = { x = 68, y = 96 },
        weight = 1,
        handler = function(state, reset)
            local magic_number = mainmemory.read_u16_le(0x000DDA)

            if magic_number == 255 then
                return 1.2 -- Equivalent to 96 frames at 60fps
            end
            
            -- Return nil to continue the challenge
            return nil
        end
    },
    {
        game_slug = "marioworld",
        rom_path = "game_data/ROMS/Super Mario World (USA).zip",
        savestate_path = "game_data/states/Super Mario World - castle level.State",
        challenge_text = "Reach the door!",
        challenge_text_pos = { x = 72, y = 96 },
        weight = 1,
        handler = function(state, reset)
            local magic_number = mainmemory.read_u16_le(0x000DDA)

            if magic_number == 255 then
                return 1.2
            end

            if magic_number == 5 then
                return 0.016
            end
            
            -- Return nil to continue the challenge
            return nil
        end
    },
    {
        game_slug = "marioworld",
        rom_path = "game_data/ROMS/Super Mario World (USA).zip",
        savestate_path = "game_data/states/Super Mario World - first boss.State",
        challenge_text = "Defeat the boss!",
        challenge_text_pos = { x = 68, y = 96 },
        weight = 1,
        handler = function(state, reset)
            local magic_number = mainmemory.read_u16_le(0x000DDA)
            local boss_number = mainmemory.read_u16_le(0x000A54)

            if magic_number == 255 or boss_number == 1 then
                return 1.2 -- Equivalent to 96 frames at 60fps
            end
            
            -- Return nil to continue the challenge
            return nil
        end
    }
}
