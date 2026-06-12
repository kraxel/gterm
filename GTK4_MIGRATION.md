# GTK4 Migration Review

This document outlines the GTK3 APIs currently used in the project that are deprecated or removed in GTK4, along with suggested replacements.

## General Changes (Affects all files)

- **`gtk_init(&argc, &argv)`**: GTK4's `gtk_init` takes no arguments. Command line parsing should be handled by `GApplication`/`GtkApplication`.
- **`gtk_widget_show_all()`**: Removed in GTK4. Widgets are visible by default. Use `gtk_widget_show()` only for top-level windows if not using `GtkApplication`.
- **`GtkContainer`**: This base class is significantly changed. Most container-specific methods (like `gtk_container_add`) are replaced by widget-specific methods (e.g., `gtk_box_append`, `gtk_window_set_child`).

## gload.c

| GTK3 API | GTK4 Equivalent / Action |
| --- | --- |
| `gtk_box_pack_start(vbox, widget, expand, fill, padding)` | `gtk_box_append(vbox, widget)`. Control expansion via `gtk_widget_set_hexpand` / `vexpand` and alignment via `set_halign` / `valign`. |
| `gtk_container_add(window, vbox)` | `gtk_window_set_child(window, vbox)` |
| `gtk_label_set_xalign(label, 0)` | `gtk_widget_set_halign(label, GTK_ALIGN_START)` |
| `gtk_widget_get_style_context()` | Style contexts are still present but their usage is more restricted. For getting colors, use the widget's color properties or CSS. |
| `gtk_style_context_get_color()` | Use `gtk_widget_get_color()` (if available in your GTK4 version) or standard CSS. |
| `gdk_cairo_set_source_rgba()` | Use `gdk_cairo_set_source_rgba()` (still exists in GDK4 but check for header changes). |
| `gtk_drawing_area` "draw" signal | Replaced by `gtk_drawing_area_set_draw_func()`. |
| `gtk_widget_get_allocated_width/height()` | Use `gtk_widget_get_width/height()`. |

## gterm.c

| GTK3 API | GTK4 Equivalent / Action |
| --- | --- |
| `GtkMenu`, `GtkMenuItem`, `GtkSeparatorMenuItem`, `GtkRadioMenuItem`, `GtkCheckMenuItem` | **Completely removed.** Use `GMenu` (menu model) and `GtkPopoverMenu`. Actions should be implemented via `GAction`. |
| `gtk_menu_popup_at_pointer()` | Replaced by `GtkPopoverMenu`. |
| `GtkGestureMultiPress` | Replaced by `GtkGestureClick`. |
| `gtk_gesture_get_last_event()` | Events are now opaque. Use `gtk_event_controller_get_current_event()`. |
| `gdk_event_get_state()` | Use `gdk_event_get_modifier_state()`. |
| `gtk_show_uri_on_window()` | Replaced by `gtk_show_uri()`. |
| `gtk_window_set_geometry_hints()` | **Removed.** GTK4 does not support geometry hints for terminal grid sizing. You may need to handle this manually by calculating minimum sizes. |
| `gtk_widget_get_preferred_size()` | Replaced by `gtk_widget_measure()`. |
| `gtk_window_resize()` | `gtk_window_set_default_size()` is preferred, or `gtk_window_present()`. |
| `key-press-event` signal | Replaced by `GtkEventControllerKey`. |
| `vte_terminal_copy_clipboard_format()` / `vte_terminal_paste_clipboard()` | VTE 0.6x+ (GTK4 versions) use different clipboard handling. Check `vte_terminal_copy_clipboard_async` and `vte_terminal_paste_clipboard_async`. |

## Build System (meson.build)

- Update dependency from `gtk+-3.0` to `gtk4`.
- Update dependency from `vte-2.91` to `vte-2.91-gtk4`.

## Recommendations

1. **Adopt GAction**: Move menu logic to `GAction` to simplify the transition to `GMenu`.
2. **UI Files**: Consider using `.ui` files (GtkBuilder) for layouts to make the transition easier.
3. **VTE Update**: Ensure you are using a version of VTE that supports GTK4.
