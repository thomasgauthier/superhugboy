
return {
    {
        game_slug = "earthbound",
        rom_path = "game_data/ROMS/EarthBound (USA).zip",
        savestate_path = "game_data/states/EarthBound - battle.State",
        challenge_text = "Win the fight!",
        challenge_text_pos = { x = 128, y = 84 },
        weight = 1,
        handler = function(state, reset)
            local snake_hp = mainmemory.read_u8(0x00A22D)
            local ness_hp = mainmemory.read_u8(0x009A15)
            local flee_check = mainmemory.read_u8(0x001085)

            if ness_hp == 0 or flee_check == 148 then
                return 0 
            end

            if snake_hp == 0 then
                return 3.8 -- Switch after 1.6 seconds (~96 frames at 60fps)
            end
            
            -- Return nil to continue the challenge
            return nil
        end
    }
}
