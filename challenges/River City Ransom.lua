
return {
    {
        game_slug = "rivercityransom",
        rom_path = "game_data/ROMS/River City Ransom (USA).zip",
        savestate_path = "game_data/states/River City Ransom - level 1.State",
        challenge_text = "Finish the screen!",
        challenge_text_pos = { x = 128, y = 96 },
        weight = 1,
        handler = function(state, reset)
            local player_hp = memory.readbyte(0x04BF)
            local screen_transition = memory.readbyte(0x0042)
                        
            if player_hp == 0 then
                return 0.8
            end
            
            if screen_transition == 1 then
                return 0.3
            end
            
            return nil
        end
    }
} 