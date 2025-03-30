
return {
    {
        game_slug = "marioworldinterlude",
        rom_path = "game_data/ROMS/Super Mario World (USA).zip",
        savestate_path = "game_data/states/Super Mario World - interlude.State",
        challenge_text = "",
        challenge_text_pos = {x = 30, y = 56},
        weight = 1,
        handler = function(state, reset)
            local text = "Are you comfortable?\nAre you having fun?"
            gui.drawRectangle(24, 39, 206, 96, "black", "black")
            gui.drawText(128, 46, "CHECKPOINT!", "#FFFB00", "#8C5918", 16, nil, "bold", "center")
            gui.drawText(128, 66, "Talk to each other!", "#5AAAF7", nil, 14, nil, "bold", "center")
            gui.drawText(128, 86, text, "white", nil, nil, nil, nil, "center")
            gui.drawText(128, 113, "Continue?", "white", nil, 14, nil, "bold", "center")
            local magic_number = mainmemory.read_u16_le(0x000DDA)
            local popup_id = mainmemory.read_u16_le(0x0013D2)

            if popup_id == 1 then
                return 0.8
            end

            if magic_number == 255 then
                return 1.2 -- Equivalent to 96 frames at 60fps
            end

            -- Return nil to continue the challenge
            return nil
        end
    }
}
