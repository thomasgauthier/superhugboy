-- A Link to the Past Cell Challenge Handler

return {
    {
        game_slug = "alttp_cell",
        rom_path = "game_data/ROMS/Legend of Zelda, The - A Link to the Past (USA).zip",
        savestate_path = "game_data/states/A Link to the Past - mini boss.State",
        challenge_text = "Escape from the dungeon!",
        challenge_text_pos = { x = 100, y = 100 },
        weight = 1,
        handler = function(state, reset)
            local music_id = mainmemory.read_u16_le(0x000132)
            local player_state = mainmemory.read_u16_le(0x00005E)

            if music_id == 61712 or player_state == 2 then
                return 1.6 -- Equivalent to 96 frames at 60fps
            end

            if music_id == 6416 then
                return 1.6 -- Equivalent to 96 frames at 60fps
            end
            
            -- Return nil to continue the challenge
            return nil
        end
    }
}
