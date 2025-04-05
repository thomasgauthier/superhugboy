
return {
    {
        game_slug = "gradius",
        rom_path = "game_data/ROMS/Gradius (USA).zip",
        savestate_path = "game_data/states/Gradius - boss.State",
        challenge_text = "Defeat the boss!",
        challenge_text_pos = { x = 128, y = 96 },
        weight = 1,
        handler = function(state, reset)
            local music_id = mainmemory.readbyte(0x001C)
            local player_state = mainmemory.readbyte(0x0100)

            if player_state == 2 then
                return 1.6
            end

            if music_id == 147 then
                return 0 
            
            return nil
        end
    }
}
