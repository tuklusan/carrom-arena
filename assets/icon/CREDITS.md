# Icon credit

`app_icon.png` and `app_icon.ico` are original artwork made for this project (2026-09-27): a polished-gold striker in
front of a black coin, on a dark rounded-square badge, echoing the striker's own on-screen look (`draw_striker_polished`
in `src/render/striker_draw.h`). Generated procedurally (Pillow) rather than hand-drawn or sourced from a third party;
no separate licence applies beyond the project's own.

`app_icon.ico` (256/128/64/48/32/16 px) is compiled into the Windows exe as its file icon via `app_icon.rc`
(`src/CMakeLists.txt`). `app_icon.png` is embedded into the executable at build time, the same way the audio in
`assets/audio/` is, and set as the running window's icon (`SetWindowIcon`, `renderer_create`) on every platform.
To change the icon, replace both files with new ones of the same name (any size; `app_icon.ico` should keep several
sizes) and rebuild.
