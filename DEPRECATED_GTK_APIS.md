# Deprecated GTK / GDK / VTE APIs in gterm and gload (TODO List)

This document serves as a TODO list detailing remaining deprecated or legacy GTK, GDK, and VTE APIs in `gterm` and `gload` that need modernization. It is categorized by whether the changes can be done **while staying on GTK 3** or if they require upgrading to **GTK 4**.

---

## Executive Summary

The project currently uses **GTK 3** (`gtk+-3.0`) and **VTE 2.91** (`vte-2.91`). The items below represent remaining refactoring work to modernize the codebase for GTK 3 and prepare for a future GTK 4 migration.

---

## 1. Modernization TODOs (Can Be Done While Staying on GTK 3)

The following items can be refactored in GTK 3 without breaking compatibility or requiring GTK 4:

### 1.1 Keyboard Event Handling (`key-press-event` -> `GtkEventControllerKey`)
- **Location**: `gterm.c:139`, `gterm.c:417`
- **Current GTK 3 Code**: Connects to the `"key-press-event"` signal on `gt->terminal` and casts event parameter to `GdkEventKey*`.
- **TODO**: Attach a `GtkEventControllerKey` controller (`gtk_event_controller_key_new(widget)`) to the widget and handle the `"key-pressed"` signal instead of low-level `"key-press-event"` signal handlers.

### 1.2 URI Opening (`gtk_show_uri_on_window`)
- **Location**: `gterm.c:186`
- **Current GTK 3 Code**:
  ```c
  gtk_show_uri_on_window(GTK_WINDOW(gt->window), url, gdk_event_get_time(event), NULL);
  ```
- **TODO**: In GTK 4, `gtk_show_uri_on_window()` is replaced by `gtk_show_uri()` (or `g_app_info_launch_default_for_uri_async()`). Update URI opening call to be compatible with GTK 4.

### 1.3 Widget Visibility (`gtk_widget_show_all` -> `gtk_widget_show`)
- **Location**: `gterm.c:431`, `gload.c:231`
- **Current GTK 3 Code**: Calls `gtk_widget_show_all(gt->window)` or `gtk_widget_show_all(gl->window)`.
- **TODO**: Replace `gtk_widget_show_all()` with `gtk_widget_show()` / explicit child visibility (`gtk_widget_show_all()` is removed in GTK 4 where widgets are visible by default).

### 1.4 Widget Alignment / Packing (`gtk_label_set_xalign` vs `gtk_widget_set_halign`)
- **Location**: `gload.c:209`
- **Current GTK 3 Code**: `gtk_label_set_xalign(GTK_LABEL(gl->label), 0);`
- **TODO**: Replace `gtk_label_set_xalign()` with `gtk_widget_set_halign(gl->label, GTK_ALIGN_START)`, which works in GTK 3 and is the standard alignment approach in GTK 4.

---

## 2. Upgrade TODOs (Requires Upgrade to GTK 4)

The following changes **cannot** be fully converted in GTK 3 because the GTK 4 replacement APIs do not exist in GTK 3:

### 2.1 Container API (`gtk_container_add` / `gtk_box_pack_start` -> `gtk_window_set_child` / `gtk_box_append`)
- **Locations**: `gterm.c:419`, `gload.c:203, 210, 229`
- **Reason / TODO**: `GtkContainer` and `gtk_box_pack_start` were removed in GTK 4. Update to `gtk_window_set_child()` and `gtk_box_append()` when upgrading to GTK 4.

### 2.2 Drawing Area Draw Signal (`draw` -> `gtk_drawing_area_set_draw_func`)
- **Location**: `gload.c:227`
- **Reason / TODO**: The `"draw"` signal (Cairo) was removed in GTK 4 in favor of `gtk_drawing_area_set_draw_func()`. Update drawing logic when upgrading to GTK 4.

### 2.3 Window Geometry Hints (`gtk_window_set_geometry_hints`)
- **Location**: `gterm.c:233, 278, 427`
- **Reason / TODO**: `gtk_window_set_geometry_hints()` was removed in GTK 4. Replace with window layout measures and constraints when upgrading to GTK 4.

---

## 3. TODO Summary Matrix

| API / Feature | Current Code | Target / Replacement | Target GTK Version |
|---|---|---|:---:|
| **Key Event Signals** | `"key-press-event"` | `GtkEventControllerKey` (`"key-pressed"`) | **GTK 3 / 4** |
| **URI Opening** | `gtk_show_uri_on_window()` | `gtk_show_uri()` | **GTK 3 / 4** |
| **Widget Showing** | `gtk_widget_show_all()` | `gtk_widget_show()` / default child visibility | **GTK 3 / 4** |
| **Label Alignment** | `gtk_label_set_xalign()` | `gtk_widget_set_halign()` | **GTK 3 / 4** |
| **Box Packing** | `gtk_container_add()`, `gtk_box_pack_start()` | `gtk_box_append()`, `gtk_window_set_child()` | **GTK 4** |
| **Drawing Area** | `"draw"` signal (`cairo_t*`) | `gtk_drawing_area_set_draw_func()` | **GTK 4** |
| **Geometry Hints** | `gtk_window_set_geometry_hints()` | Window layout measures | **GTK 4** |
