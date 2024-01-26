// Copyright (c) 2022 Macro.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "office/lok_callback.h"

#include <cstddef>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "LibreOfficeKit/LibreOfficeKitEnums.h"
#include "base/logging.h"
#include "gin/converter.h"
#include "ui/gfx/geometry/rect.h"
#include "v8-primitive.h"
#include "v8/include/v8-exception.h"

namespace electron::office::lok_callback {

void SkipWhitespace(std::string_view::const_iterator& target,
                    std::string_view::const_iterator end) {
  while (target < end && std::iswspace(*target))
    ++target;
}

// simple, fast parse for an unsigned long
// target comes from a stored iterator from a string_view,
// end is the end iterator from a string_view,
// value is the optional initial value
uint64_t ParseLong(std::string_view::const_iterator& target,
                   std::string_view::const_iterator end,
                   uint64_t value = 0) {
  for (char c; target < end && (c = *target ^ '0') <= 9; ++target) {
    value = value * 10 + c;
  }
  return value;
}

// simple, fast parse for a ,-separated list of longs, optionally terminated
// with a ;
// target comes from a stored iterator from a string_view
// end is the end iterator from a string_view
std::vector<uint64_t> ParseCSV(std::string_view::const_iterator& target,
                               std::string_view::const_iterator end) {
  std::vector<uint64_t> result;
  while (target < end) {
    if (*target == ';') {
      ++target;
      break;
    }
    if (*target == ',')
      ++target;
    SkipWhitespace(target, end);

    // no number follows, finish
    if ((*target ^ '0') > 9) {
      return result;
    }

    result.emplace_back(ParseLong(target, end));
  }

  return result;
}

// simple, fast parse for a ;-separated list of ,-separated lists of longs,
// optionally terminated with a ;
// target comes from a stored iterator from a string_view
// end is the end iterator from a string_view
std::vector<std::vector<uint64_t>> ParseMultipleCSV(
    std::string_view::const_iterator& target,
    std::string_view::const_iterator end) {
  std::vector<std::vector<uint64_t>> result;
  while (target < end) {
    result.emplace_back(ParseCSV(target, end));
  }

  return result;
}

void SkipNonNumeric(std::string_view::const_iterator& target,
                    std::string_view::const_iterator end) {
  while ((*target ^ '0') > 9) {
    ++target;
  }
}

// simple, fast parse for a ,-separated list of longs, optionally terminated
// with a ;
// target comes from a stored iterator from a string_view
// end is the end iterator from a string_view
gfx::Rect ParseRect(std::string_view::const_iterator& target,
                    std::string_view::const_iterator end) {
  SkipNonNumeric(target, end);
  if (target == end)
    return gfx::Rect();

  long x = ParseLong(target, end);
  SkipNonNumeric(target, end);
  long y = ParseLong(target, end);
  SkipNonNumeric(target, end);
  long w = ParseLong(target, end);
  SkipNonNumeric(target, end);
  long h = ParseLong(target, end);

  return gfx::Rect(x, y, w, h);
}

// simple, fast parse for a ;-separated list of ,-separated lists of longs,
// optionally terminated with a ;
// target comes from a stored iterator from a string_view
// end is the end iterator from a string_view
std::vector<gfx::Rect> ParseMultipleRects(
    std::string_view::const_iterator& target,
    std::string_view::const_iterator end,
    size_t size) {
  std::vector<gfx::Rect> result;
  result.reserve(size);

  while (target < end) {
    result.emplace_back(ParseRect(target, end));
  }

  return result;
}

int EventStringToType(const std::u16string& eventString) {
  static std::unordered_map<std::u16string, int> EventStringToTypeMap = {
      {u"invalidate_tiles", LOK_CALLBACK_INVALIDATE_TILES},
      {u"invalidate_visible_cursor", LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR},
      {u"text_selection", LOK_CALLBACK_TEXT_SELECTION},
      {u"text_selection_start", LOK_CALLBACK_TEXT_SELECTION_START},
      {u"text_selection_end", LOK_CALLBACK_TEXT_SELECTION_END},
      {u"cursor_visible", LOK_CALLBACK_CURSOR_VISIBLE},
      {u"view_cursor_visible", LOK_CALLBACK_VIEW_CURSOR_VISIBLE},
      {u"graphic_selection", LOK_CALLBACK_GRAPHIC_SELECTION},
      {u"graphic_view_selection", LOK_CALLBACK_GRAPHIC_VIEW_SELECTION},
      {u"cell_cursor", LOK_CALLBACK_CELL_CURSOR},
      {u"hyperlink_clicked", LOK_CALLBACK_HYPERLINK_CLICKED},
      {u"mouse_pointer", LOK_CALLBACK_MOUSE_POINTER},
      {u"state_changed", LOK_CALLBACK_STATE_CHANGED},
      {u"status_indicator_start", LOK_CALLBACK_STATUS_INDICATOR_START},
      {u"status_indicator_set_value", LOK_CALLBACK_STATUS_INDICATOR_SET_VALUE},
      {u"status_indicator_finish", LOK_CALLBACK_STATUS_INDICATOR_FINISH},
      {u"search_not_found", LOK_CALLBACK_SEARCH_NOT_FOUND},
      {u"document_size_changed", LOK_CALLBACK_DOCUMENT_SIZE_CHANGED},
      {u"set_part", LOK_CALLBACK_SET_PART},
      {u"search_result_selection", LOK_CALLBACK_SEARCH_RESULT_SELECTION},
      {u"document_password", LOK_CALLBACK_DOCUMENT_PASSWORD},
      {u"document_password_to_modify",
       LOK_CALLBACK_DOCUMENT_PASSWORD_TO_MODIFY},
      {u"context_menu", LOK_CALLBACK_CONTEXT_MENU},
      {u"invalidate_view_cursor", LOK_CALLBACK_INVALIDATE_VIEW_CURSOR},
      {u"text_view_selection", LOK_CALLBACK_TEXT_VIEW_SELECTION},
      {u"cell_view_cursor", LOK_CALLBACK_CELL_VIEW_CURSOR},
      {u"cell_address", LOK_CALLBACK_CELL_ADDRESS},
      {u"cell_formula", LOK_CALLBACK_CELL_FORMULA},
      {u"uno_command_result", LOK_CALLBACK_UNO_COMMAND_RESULT},
      {u"error", LOK_CALLBACK_ERROR},
      {u"view_lock", LOK_CALLBACK_VIEW_LOCK},
      {u"redline_table_size_changed", LOK_CALLBACK_REDLINE_TABLE_SIZE_CHANGED},
      {u"redline_table_entry_modified",
       LOK_CALLBACK_REDLINE_TABLE_ENTRY_MODIFIED},
      {u"invalidate_header", LOK_CALLBACK_INVALIDATE_HEADER},
      {u"comment", LOK_CALLBACK_COMMENT},
      {u"ruler_update", LOK_CALLBACK_RULER_UPDATE},
      {u"window", LOK_CALLBACK_WINDOW},
      {u"validity_list_button", LOK_CALLBACK_VALIDITY_LIST_BUTTON},
      {u"validity_input_help", LOK_CALLBACK_VALIDITY_INPUT_HELP},
      {u"clipboard_changed", LOK_CALLBACK_CLIPBOARD_CHANGED},
      {u"context_changed", LOK_CALLBACK_CONTEXT_CHANGED},
      {u"signature_status", LOK_CALLBACK_SIGNATURE_STATUS},
      {u"profile_frame", LOK_CALLBACK_PROFILE_FRAME},
      {u"cell_selection_area", LOK_CALLBACK_CELL_SELECTION_AREA},
      {u"cell_auto_fill_area", LOK_CALLBACK_CELL_AUTO_FILL_AREA},
      {u"table_selected", LOK_CALLBACK_TABLE_SELECTED},
      {u"reference_marks", LOK_CALLBACK_REFERENCE_MARKS},
      {u"jsdialog", LOK_CALLBACK_JSDIALOG},
      {u"calc_function_list", LOK_CALLBACK_CALC_FUNCTION_LIST},
      {u"tab_stop_list", LOK_CALLBACK_TAB_STOP_LIST},
      {u"form_field_button", LOK_CALLBACK_FORM_FIELD_BUTTON},
      {u"invalidate_sheet_geometry", LOK_CALLBACK_INVALIDATE_SHEET_GEOMETRY},
      {u"document_background_color", LOK_CALLBACK_DOCUMENT_BACKGROUND_COLOR},
      {u"lok_command_blocked", LOK_COMMAND_BLOCKED},
      {u"sc_follow_jump", LOK_CALLBACK_SC_FOLLOW_JUMP},
      {u"content_control", LOK_CALLBACK_CONTENT_CONTROL},
      {u"print_ranges", LOK_CALLBACK_PRINT_RANGES},
      {u"fonts_missing", LOK_CALLBACK_FONTS_MISSING},
      {u"macro_colorizer", LOK_CALLBACK_MACRO_COLORIZER},
      {u"macro_overlay", LOK_CALLBACK_MACRO_OVERLAY},
      {u"media_shape", LOK_CALLBACK_MEDIA_SHAPE},
      {u"export_file", LOK_CALLBACK_EXPORT_FILE},
      {u"view_render_state", LOK_CALLBACK_VIEW_RENDER_STATE},
      {u"application_background_color",
       LOK_CALLBACK_APPLICATION_BACKGROUND_COLOR},
      {u"a11y_focus_changed", LOK_CALLBACK_A11Y_FOCUS_CHANGED},
      {u"a11y_caret_changed", LOK_CALLBACK_A11Y_CARET_CHANGED},
      {u"a11y_text_selection_changed",
       LOK_CALLBACK_A11Y_TEXT_SELECTION_CHANGED},
      {u"color_palettes", LOK_CALLBACK_COLOR_PALETTES},
      {u"document_password_reset", LOK_CALLBACK_DOCUMENT_PASSWORD_RESET},
      {u"a11y_focused_cell_changed", LOK_CALLBACK_A11Y_FOCUSED_CELL_CHANGED},
      {u"ready", 300}  // this is a special event internal to ELOK
  };

  auto it = EventStringToTypeMap.find(eventString);
  if (it != EventStringToTypeMap.end()) {
    return it->second;
  }
  return -1;  // Example: Not found, you can change this to suit your needs.
}

bool IsTypeJSON(int type) {
  switch (static_cast<LibreOfficeKitCallbackType>(type)) {
    case LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR:  // INVALIDATE_VISIBLE_CURSOR
                                                  // may also be CSV
    case LOK_CALLBACK_CURSOR_VISIBLE:
    case LOK_CALLBACK_VIEW_CURSOR_VISIBLE:
    case LOK_CALLBACK_GRAPHIC_SELECTION:
    case LOK_CALLBACK_GRAPHIC_VIEW_SELECTION:
    case LOK_CALLBACK_SET_PART:
    case LOK_CALLBACK_SEARCH_RESULT_SELECTION:
    case LOK_CALLBACK_CONTEXT_MENU:
    case LOK_CALLBACK_INVALIDATE_VIEW_CURSOR:
    case LOK_CALLBACK_TEXT_VIEW_SELECTION:
    case LOK_CALLBACK_CELL_VIEW_CURSOR:
    case LOK_CALLBACK_UNO_COMMAND_RESULT:
    case LOK_CALLBACK_ERROR:
    case LOK_CALLBACK_VIEW_LOCK:
    case LOK_CALLBACK_REDLINE_TABLE_SIZE_CHANGED:
    case LOK_CALLBACK_REDLINE_TABLE_ENTRY_MODIFIED:
    case LOK_CALLBACK_COMMENT:
    case LOK_CALLBACK_RULER_UPDATE:
    case LOK_CALLBACK_WINDOW:
    case LOK_CALLBACK_VALIDITY_INPUT_HELP:
    case LOK_CALLBACK_CLIPBOARD_CHANGED:
    case LOK_CALLBACK_REFERENCE_MARKS:
    case LOK_CALLBACK_JSDIALOG:
    case LOK_CALLBACK_MACRO_OVERLAY:
    case LOK_CALLBACK_MACRO_COLORIZER:
      // TODO: Not Certain, limited documentation {
    case LOK_CALLBACK_CALC_FUNCTION_LIST:
    case LOK_CALLBACK_TAB_STOP_LIST:
    case LOK_COMMAND_BLOCKED:
    case LOK_CALLBACK_TABLE_SELECTED:
      // }

    case LOK_CALLBACK_FORM_FIELD_BUTTON:
    case LOK_CALLBACK_CONTENT_CONTROL:
    case LOK_CALLBACK_PRINT_RANGES:
    case LOK_CALLBACK_STATUS_INDICATOR_SET_VALUE:
      return true;
    default:
      return false;
  }
}

/* Is comma-separated number values. A semi-colon indicates a new array */
bool IsTypeCSV(int type) {
  switch (static_cast<LibreOfficeKitCallbackType>(type)) {
    case LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR:  // INVALIDATE_VISIBLE_CURSOR
                                                  // may also be JSON
    case LOK_CALLBACK_INVALIDATE_TILES:
    case LOK_CALLBACK_TEXT_SELECTION_START:
    case LOK_CALLBACK_TEXT_SELECTION_END:
    case LOK_CALLBACK_CELL_CURSOR:
    case LOK_CALLBACK_DOCUMENT_SIZE_CHANGED:
    case LOK_CALLBACK_VALIDITY_LIST_BUTTON:
    case LOK_CALLBACK_CELL_SELECTION_AREA:
    case LOK_CALLBACK_CELL_AUTO_FILL_AREA:
    case LOK_CALLBACK_SC_FOLLOW_JUMP:
      return true;
    default:
      return false;
  }
}

/* Is comma-separated number values. A semi-colon indicates a new array */
bool IsTypeMultipleCSV(int type) {
  switch (static_cast<LibreOfficeKitCallbackType>(type)) {
    case LOK_CALLBACK_TEXT_SELECTION:
      return true;
    default:
      return false;
  }
}

v8::Local<v8::Value> ParseJSON(v8::Isolate* isolate,
                               v8::Local<v8::String> json) {
  if (json->Length() == 0)
    return v8::Null(isolate);

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  auto maybe_value = v8::JSON::Parse(context, json);

  if (maybe_value.IsEmpty() || try_catch.HasCaught()) {
    v8::Local<v8::Message> message(try_catch.Message());
    if (!message.IsEmpty()) {
      v8::String::Utf8Value utf8(isolate, message->Get());
      v8::String::Utf8Value utf8Json(isolate, json);
      LOG(ERROR) << "Unable to parse callback JSON:"
                 << std::string(*utf8, utf8.length());
      LOG(ERROR) << std::string(*utf8Json, utf8Json.length());
    }

    return v8::Null(isolate);
  }

  return maybe_value.ToLocalChecked();
}

// The weirdest of the types:
// A pair of the ([x,y,width,height,angle], JSON string)
// See docs of LOK_CALLBACK_GRAPHIC_SELECTION enum type for more details
v8::Local<v8::Value> GraphicSelectionPayloadToLocalValue(v8::Isolate* isolate,
                                                         const char* payload) {
  std::string_view payload_sv(payload);
  std::string_view::const_iterator start = payload_sv.begin();
  std::string_view::const_iterator end = payload_sv.end();
  std::vector<uint64_t> numbers = ParseCSV(start, end);
  auto numbers_v8 =
      gin::Converter<std::vector<uint64_t>>::ToV8(isolate, numbers);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  auto result_array = v8::Array::New(isolate, 2);
  std::ignore = result_array->Set(context, 0, numbers_v8);

  if (start == end) {
    std::ignore = result_array->Set(context, 1, v8::Null(isolate));
    return result_array;
  }

  v8::MaybeLocal<v8::String> maybe_string = v8::String::NewFromUtf8(
      isolate, start, v8::NewStringType::kNormal, end - start);
  if (maybe_string.IsEmpty()) {
    std::ignore = result_array->Set(context, 1, v8::Null(isolate));
    return result_array;
  }
  v8::Local<v8::String> string = maybe_string.ToLocalChecked();
  std::ignore = result_array->Set(context, 1, ParseJSON(isolate, string));
  return result_array;
}

v8::Local<v8::Value> PayloadToLocalValue(v8::Isolate* isolate,
                                         int type,
                                         const char* payload) {
  if (payload == nullptr) {
    return v8::Null(isolate);
  }

  LibreOfficeKitCallbackType cast_type =
      static_cast<LibreOfficeKitCallbackType>(type);
  if (cast_type == LOK_CALLBACK_GRAPHIC_SELECTION) {
    return GraphicSelectionPayloadToLocalValue(isolate, payload);
  }

  // INVALIDATE_VISIBLE_CURSOR may also be JSON, so check if the payload starts
  // with '{'
  if (IsTypeCSV(type) && payload[0] != '{') {
    std::string_view payload_sv(payload);
    std::string_view::const_iterator start = payload_sv.begin();
    std::vector<uint64_t> result = ParseCSV(start, payload_sv.end());
    return gin::Converter<std::vector<uint64_t>>::ToV8(isolate, result);
  }

  if (IsTypeMultipleCSV(type)) {
    std::string_view payload_sv(payload);
    std::string_view::const_iterator start = payload_sv.begin();
    std::vector<std::vector<uint64_t>> result =
        ParseMultipleCSV(start, payload_sv.end());
    return gin::Converter<std::vector<std::vector<uint64_t>>>::ToV8(isolate,
                                                                    result);
  }

  v8::MaybeLocal<v8::String> maybe_string =
      v8::String::NewFromUtf8(isolate, payload);
  if (maybe_string.IsEmpty()) {
    return v8::Null(isolate);
  }

  v8::Local<v8::String> string = maybe_string.ToLocalChecked();

  if (!IsTypeJSON(type) &&
      !(cast_type == LOK_CALLBACK_STATE_CHANGED && payload[0] == '{')) {
    return string;
  }

  return ParseJSON(isolate, string);
}

/* Remaining odd/string types:
    case LOK_CALLBACK_MOUSE_POINTER:
    case LOK_CALLBACK_STATUS_INDICATOR_START:
    case LOK_CALLBACK_STATUS_INDICATOR_FINISH:
    case LOK_CALLBACK_SEARCH_NOT_FOUND:
    case LOK_CALLBACK_DOCUMENT_PASSWORD:
    case LOK_CALLBACK_DOCUMENT_PASSWORD_TO_MODIFY:
    case LOK_CALLBACK_CELL_ADDRESS:
    case LOK_CALLBACK_CELL_FORMULA:
    case LOK_CALLBACK_INVALIDATE_HEADER:
    case LOK_CALLBACK_CONTEXT_CHANGED:
    case LOK_CALLBACK_SIGNATURE_STATUS:
    case LOK_CALLBACK_PROFILE_FRAME:
    case LOK_CALLBACK_INVALIDATE_SHEET_GEOMETRY:
    case LOK_CALLBACK_DOCUMENT_BACKGROUND_COLOR:
*/

}  // namespace electron::office::lok_callback
