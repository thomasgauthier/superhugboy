-- Super Metroid Challenge Handler

return {
    {
        game_slug = "supermetroid_escape",
        rom_path = "game_data/ROMS/Super Metroid (Japan, USA) (En,Ja).zip",
        savestate_path = "game_data/states/Super Metroid - First Escape.State",
        challenge_text = "Escape!",
        challenge_text_pos = { x = 128, y = 96 },
        weight = 1,
        handler = function(state, reset)
            local game_state = mainmemory.read_u16_le(0x000998)

            if game_state == 32 then
                return 1.6 -- Switch after 1.6 seconds (~96 frames at 60fps)
            end

            if game_state == 35 then
                return 1.6 -- Switch after 1.6 seconds (~96 frames at 60fps)
            end
            
            -- Return nil to continue the challenge
            return nil
        end
    }
}
