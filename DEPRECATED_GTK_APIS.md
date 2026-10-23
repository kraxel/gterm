# Deprecated GTK / GDK / VTE APIs in gterm and gload

This document details deprecated GTK, GDK, and VTE APIs found in `gterm` and `gload`, categorized by whether they can be modernized **while staying on GTK 3** or if they require upgrading to **GTK 4**.

---

## Executive Summary

The project currently uses **GTK 3** (`gtk+-3.0`) and **VTE 2.91** (`vte-2.91`). While the codebase builds cleanly without `-Wdeprecated-declarations` warnings under standard GTK 3 settings, several APIs in use are either deprecated in GTK 3 / VTE, or have modern GTK 3 equivalents that simplify a future GTK 4 migration.

---

## 1. APIs That CAN Be Modernized While Staying on GTK 3

The following items can be refactored right now in GTK 3 without breaking compatibility or requiring GTK 4:

### 1.1 VTE Window Title Signal (`window-title-changed` -> `termprop-changed::xterm.title`)
- **Location**: `gterm.c:132`, `gterm.c:514`
- **Current GTK 3 Code**:
  ```c
  #if VTE_CHECK_VERSION(0, 78, 0)
      g_signal_connect(G_OBJECT(gt->terminal), "termprop-changed::xterm.title",
                       G_CALLBACK(gterm_vte_termprop_changed), gt);
  #else
      g_signal_connect(G_OBJECT(gt->terminal), "window-title-changed",
                       G_CALLBACK(gterm_vte_window_title_changed), gt);
  #endif
  ```
- **Modernization in GTK 3**:
  - In VTE 0.78+, the `window-title-changed` signal and `window-title` property are deprecated in favor of terminal properties (`termprop`).
  - The fallback `#if VTE_CHECK_VERSION(0, 78, 0)` is already implemented in `gterm.c`, using `termprop-changed::xterm.title`.

### 1.2 `gtk_show_uri_on_window`
- **Location**: `gterm.c:202`
- **Current Usage**:
  ```c
  gtk_show_uri_on_window(GTK_WINDOW(gt->window), url, gdk_event_get_time(event), NULL);
  ```
- **Status in GTK 3**: Deprecated in GTK 3.22 in favor of `gtk_show_uri_on_window()` or `g_app_info_launch_default_for_uri_async()`.
- **Modernization in GTK 3**:
  - `gtk_show_uri_on_window()` is supported in GTK 3.22+.

### 1.3 Popup Menu System (`GtkMenu` / `GtkMenuItem` -> `GMenu` / `GtkPopoverMenu`)
- **Location**: `gterm.c:212, 419-453`
- **Current GTK 3 Code**: Uses `GtkMenu`, `GtkMenuItem`, `GtkCheckMenuItem`, `GtkRadioMenuItem`, and `gtk_menu_popup_at_pointer()`.
- **Modernization in GTK 3**:
  - GTK 3 supports constructing menus using `GMenuModel` / `GMenu` and actions (`GAction` / `GSimpleActionGroup`).
  - Menus can be displayed using `gtk_menu_new_from_model()` or `GtkPopoverMenu` (`gtk_popover_menu_new_from_model()`) in GTK 3.22+.
  - This completely decouples the menu definition from legacy GTK 3 menu widgets and makes GTK 4 migration seamless.

### 1.4 Keyboard Event Handling (`key-press-event` -> `GtkEventControllerKey`)
- **Location**: `gterm.c:155, 517`
- **Current GTK 3 Code**: Connects to the `"key-press-event"` signal on `gt->terminal` and casts event parameter to `GdkEventKey*`.
- **Modernization in GTK 3**:
  - GTK 3.24+ includes `GtkEventControllerKey` (`gtk_event_controller_key_new(widget)`).
  - Can attach a `GtkEventControllerKey` controller to the widget and handle the `"key-pressed"` signal instead of low-level `key-press-event` signal handlers.

### 1.5 Widget Visibility (`gtk_widget_show_all` -> `gtk_widget_show`)
- **Location**: `gterm.c:453, 526`, `gload.c:231`
- **Current GTK 3 Code**: Calls `gtk_widget_show_all(window)` or `gtk_widget_show_all(popup)`.
- **Modernization in GTK 3**:
  - In GTK 3, calling `gtk_widget_show(window)` or explicitly showing child widgets works and avoids relying on `gtk_widget_show_all()` (which is completely removed in GTK 4 where widgets are visible by default).

### 1.6 Widget Alignment / Packing (`gtk_label_set_xalign` vs `gtk_widget_set_halign`)
- **Location**: `gload.c:209`
- **Current GTK 3 Code**: `gtk_label_set_xalign(GTK_LABEL(gl->label), 0);`
- **Modernization in GTK 3**:
  - `gtk_widget_set_halign(gl->label, GTK_ALIGN_START)` works in GTK 3 and is the standard alignment approach in GTK 4.

---

## 2. APIs Requiring Upgrade to GTK 4

The following changes **cannot** be fully converted in GTK 3 because the GTK 4 replacement APIs do not exist in GTK 3:

### 2.1 Container API (`gtk_container_add` / `gtk_box_pack_start` -> `gtk_window_set_child` / `gtk_box_append`)
- **Locations**: `gterm.c:424+`, `gload.c:205, 214, 229`
- **Reason**: `GtkContainer` and `gtk_box_pack_start` were removed in GTK 4. GTK 3 does not have `gtk_box_append()` or `gtk_window_set_child()`.

### 2.2 Drawing Area Draw Signal (`draw` -> `gtk_drawing_area_set_draw_func`)
- **Location**: `gload.c:227`
- **Reason**: The `"draw"` signal (Cairo) was removed in GTK 4 in favor of `gtk_drawing_area_set_draw_func()`, which does not exist in GTK 3.

### 2.3 Window Geometry Hints (`gtk_window_set_geometry_hints`)
- **Location**: `gterm.c:354, 380, 525`
- **Reason**: `gtk_window_set_geometry_hints()` was removed in GTK 4 without a direct GTK 3 drop-in replacement (GTK 4 uses grid/layout measures and constraints).

---

## 3. Summary Matrix

| API / Feature | Current Code | Modern GTK 3 Replacement | Can Modernize in GTK 3? |
|---|---|---|:---:|
| **Context Menu** | `GtkMenu` + `gtk_menu_popup_at_pointer` | `GMenu` + `GAction` + `gtk_menu_new_from_model` / `GtkPopoverMenu` | **Yes** |
| **Key Event Signals** | `"key-press-event"` | `GtkEventControllerKey` (`"key-pressed"`) | **Yes** |
| **VTE Title Signal** | `"window-title-changed"` | `"termprop-changed::xterm.title"` (VTE 0.78+) | **Yes** |
| **URI Opening** | `gtk_show_uri_on_window()` | `gtk_show_uri_on_window()` / `g_app_info_launch_default_for_uri` | **Yes** |
| **Widget Showing** | `gtk_widget_show_all()` | `gtk_widget_show()` / explicit child visibility | **Yes** |
| **Label Alignment** | `gtk_label_set_xalign()` | `gtk_widget_set_halign()` | **Yes** |
| **Box Packing** | `gtk_container_add()`, `gtk_box_pack_start()` | `gtk_box_append()`, `gtk_window_set_child()` | *No (GTK 4 only)* |
| **Drawing Area** | `"draw"` signal (`cairo_t*`) | `gtk_drawing_area_set_draw_func()` | *No (GTK 4 only)* |
| **Geometry Hints** | `gtk_window_set_geometry_hints()` | Window layout measures | *No (GTK 4 only)* |
