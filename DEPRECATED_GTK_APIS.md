# Deprecated GTK / GDK / VTE APIs in gterm and gload

This document details deprecated GTK, GDK, and VTE APIs found in the project (`gterm` and `gload`), categorized by their deprecation status in GTK 3 and their removal or deprecation in GTK 4, along with recommended modern replacements.

---

## Executive Summary

The project currently uses **GTK 3** (`gtk+-3.0`) and **VTE 2.91** (`vte-2.91`). While the codebase builds cleanly without `-Wdeprecated-declarations` warnings under standard GTK 3 settings, several APIs in use are either deprecated in GTK 3 / VTE, or have been completely removed or refactored in GTK 4. Modernizing these APIs will improve code quality and streamline future migration to GTK 4.

---

## 1. APIs Deprecated in GTK 3 and VTE

### 1.1 `gtk_show_uri_on_window`
- **Location**: `gterm.c:202`
- **Current Usage**:
  ```c
  gtk_show_uri_on_window(GTK_WINDOW(gt->window), url, gdk_event_get_time(event), NULL);
  ```
- **Status**:
  - **GTK 3**: Deprecated since version 3.22 (in favor of `gtk_show_uri_on_window` alternatives or `g_app_info_launch_default_for_uri`).
  - **GTK 4**: Removed.
- **Recommended Replacement**:
  - In GTK 3.22+: `gtk_show_uri_on_window` or `g_app_info_launch_default_for_uri_async()`.
  - In GTK 4: `GtkUriLauncher` or `gtk_show_uri()`.

### 1.2 `window-title-changed` Signal / `window-title` Property (VTE)
- **Location**: `gterm.c:132`, `gterm.c:514`
- **Current Usage**:
  ```c
  #if VTE_CHECK_VERSION(0, 78, 0)
      g_signal_connect(G_OBJECT(gt->terminal), "termprop-changed::xterm.title",
                       G_CALLBACK(gterm_vte_termprop_changed), gt);
  #else
      g_signal_connect(G_OBJECT(gt->terminal), "window-title-changed",
                       G_CALLBACK(gterm_vte_window_title_changed), gt);
  #endif
  ```
- **Status**: Deprecated in VTE 0.78+ in favor of the terminal property (`termprop`) API.
- **Recommended Replacement**:
  - Use `termprop-changed::xterm.title` signal and retrieve string via `vte_terminal_get_termprop_string()`.

---

## 2. APIs Removed or Redesigned in GTK 4

The following GTK 3 APIs are heavily used throughout `gterm` and `gload` but have been removed or completely redesigned in GTK 4:

### 2.1 Container & Packing APIs (`GtkContainer`, `GtkBox`)
- **Locations**:
  - `gterm.c:424, 429, 432, 437, 440, 447, 451, 519` (`gtk_container_add`)
  - `gload.c:205` (`gtk_container_add`)
  - `gload.c:214, 229` (`gtk_box_pack_start`)
- **Status**: `GtkContainer` class was removed in GTK 4.
- **Recommended Replacement**:
  - For `GtkWindow` or single-child widgets: `gtk_window_set_child(window, child)`.
  - For `GtkBox`: `gtk_box_append(box, child)`.
  - Replace `gtk_box_pack_start()` with `gtk_box_append()` and handle expansion using `gtk_widget_set_vexpand()` / `gtk_widget_set_hexpand()`.

### 2.2 Legacy Menu Widgets (`GtkMenu`, `GtkMenuItem`, `GtkCheckMenuItem`, `GtkRadioMenuItem`, `GtkSeparatorMenuItem`)
- **Locations**:
  - Context menu creation and popup in `gterm.c:212` (`gtk_menu_popup_at_pointer`)
  - Menu building in `gterm.c:419-453` (`gtk_menu_new`, `gtk_check_menu_item_new_with_label`, `gtk_radio_menu_item_new_with_label`, etc.)
- **Status**: The entire `GtkMenu` widget hierarchy and `gtk_menu_popup_at_pointer()` were removed in GTK 4.
- **Recommended Replacement**:
  - Define menu structures using `GMenu` / `GMenuModel`.
  - Display context menus using `GtkPopoverMenu` (`gtk_popover_menu_new_from_model`).
  - Wire actions using `GAction` / `GSimpleActionGroup`.

### 2.3 Widget Visibility (`gtk_widget_show_all`)
- **Locations**:
  - `gterm.c:453` (`gtk_widget_show_all(gt->popup)`)
  - `gterm.c:526` (`gtk_widget_show_all(gt->window)`)
  - `gload.c:231` (`gtk_widget_show_all(gl->window)`)
- **Status**: In GTK 4, widgets are visible by default. `gtk_widget_show_all()` was removed.
- **Recommended Replacement**:
  - Call `gtk_widget_show(widget)` or `gtk_widget_set_visible(widget, TRUE)` if explicit visibility control is needed.

### 2.4 Event Signals & Event Key Structures (`key-press-event`, `GdkEventKey`)
- **Locations**:
  - `gterm.c:155` (`gterm_vte_key_press` handler callback)
  - `gterm.c:517` (`g_signal_connect(..., "key-press-event", ...)`)
- **Status**: Direct event signals (`key-press-event`, `button-press-event`) and `GdkEventKey` structure access were removed in GTK 4.
- **Recommended Replacement**:
  - Use `GtkEventControllerKey` attached via `gtk_widget_add_controller()` with the `"key-pressed"` signal handler.

### 2.5 Window Geometry Hints (`gtk_window_set_geometry_hints`)
- **Locations**:
  - `gterm.c:354`, `gterm.c:380`, `gterm.c:525` (`gtk_window_set_geometry_hints`)
- **Status**: `gtk_window_set_geometry_hints()` was removed in GTK 4.
- **Recommended Replacement**:
  - Manage minimum window size via `gtk_widget_set_size_request()` or GTK 4 grid/layout measures.

### 2.6 Style Context API (`gtk_style_context_get_color`)
- **Locations**:
  - `gload.c:150` (`gtk_style_context_get_color`)
- **Status**: `gtk_style_context_get_color()` with state flags is deprecated in GTK 4.
- **Recommended Replacement**:
  - In GTK 4, use `gtk_widget_get_color()`.

### 2.7 Drawing Area Signal (`draw` signal on `GtkDrawingArea`)
- **Locations**:
  - `gload.c:227` (`g_signal_connect(gl->graph, "draw", ...)`)
  - `gload.c:138` (`gload_draw` taking `cairo_t *`)
- **Status**: The `"draw"` signal on `GtkDrawingArea` was removed in GTK 4.
- **Recommended Replacement**:
  - Use `gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(graph), gload_draw_func, user_data, NULL)`.

---

## 3. Summary Table

| API / Signal / Widget | Location(s) | GTK 3 Status | GTK 4 Status | Recommended Replacement |
|---|---|---|---|---|
| `gtk_show_uri_on_window()` | `gterm.c:202` | Deprecated (3.22) | Removed | `GtkUriLauncher` / `gtk_show_uri()` |
| `gtk_container_add()` | `gterm.c:424+`, `gload.c:205` | Active | Removed | `gtk_window_set_child()`, `gtk_box_append()` |
| `gtk_box_pack_start()` | `gload.c:214, 229` | Active | Removed | `gtk_box_append()` |
| `GtkMenu`, `GtkMenuItem`, `GtkCheckMenuItem`, `GtkRadioMenuItem` | `gterm.c:419-453` | Active | Removed | `GMenu` / `GMenuModel` + `GtkPopoverMenu` |
| `gtk_menu_popup_at_pointer()` | `gterm.c:212` | Active | Removed | `GtkPopoverMenu` |
| `gtk_widget_show_all()` | `gterm.c`, `gload.c` | Active | Removed | Default visible in GTK 4 / `gtk_widget_set_visible()` |
| `"key-press-event"` signal | `gterm.c:517` | Active | Removed | `GtkEventControllerKey` |
| `gtk_window_set_geometry_hints()` | `gterm.c:354, 380, 525` | Active | Removed | Modern layout sizing / grid constraints |
| `gtk_style_context_get_color()` | `gload.c:150` | Active (GTK 3) | Deprecated (4.10) | `gtk_widget_get_color()` |
| `GtkDrawingArea` `"draw"` signal | `gload.c:227` | Active | Removed | `gtk_drawing_area_set_draw_func()` |
| `window-title-changed` VTE signal | `gterm.c:514` | Deprecated in VTE | Deprecated | `termprop-changed::xterm.title` |
