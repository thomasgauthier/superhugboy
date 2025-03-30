-- A Link to the Past Cell Challenge Handler

return {
    {
        game_slug = "alttp_cell",
        rom_path = "game_data/ROMS/Legend of Zelda, The - A Link to the Past (USA).zip",
        savestate_path = "game_data/states/A Link to the Past - mini boss.State",
        challenge_text = "Free the princess!",
        challenge_text_pos = { x = 128, y = 96 },
        weight = 1,
        handler = function(state, reset)
            local music_id = mainmemory.read_u16_le(0x000132)
            local player_state = mainmemory.read_u16_le(0x00005E)

            -- exit dungeon
            if player_state == 2 then
                reset(0.2)
            end


            -- death
            if music_id == 61712 then
                return 1.1
            end

            -- victory (defeat mini boss)
            if music_id == 6416 then
                return 1.6 -- Equivalent to 96 frames at 60fps
            end
            
            -- Return nil to continue the challenge
            return nil
        end
    }
}
