
return {
    {
        game_slug = "metroid_classic",
        rom_path = "game_data/ROMS/Metroid (USA).zip",
        savestate_path = "game_data/states/Metroid - level 1.State",
        challenge_text = "Reach the door!",
        challenge_text_pos = { x = 128, y = 96 },
        weight = 1,
        handler = function(state, reset)
            local player_hp_units = memory.readbyte(0x0106)
            local player_hp_tens = memory.readbyte(0x0107)
            local door_transition = memory.readbyte(0x0056)
                        
            if player_hp_units == 0 and player_hp_tens == 0 then
                return 1.0
            end
            
            if door_transition ~= 0 then
                return 0.8
            end
            
            return nil
        end
    }
} 