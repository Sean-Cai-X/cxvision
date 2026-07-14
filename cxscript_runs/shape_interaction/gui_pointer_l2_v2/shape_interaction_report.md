# Shape Interaction Test Report

## Summary

- Total Cases: 10
- Pass: 1
- Fail: 9
- Pending Writeback: 0
- Missing Tools: 0

## Case Details

### gui_create_point

- Tool: point_pick
- Status: created
- Expected Handle: 
- Actual Handle: create_shape
- Geometry Assertion: 
- Reason: expected created_kind=PointsShape, actual=Points

### gui_create_line_drag_release

- Tool: scan_line
- Status: failed
- Expected Handle: 
- Actual Handle: create_shape
- Geometry Assertion: 
- Reason: expected created_kind=LineShape, actual=

### gui_create_rect_drag_release

- Tool: roi_rect
- Status: failed
- Expected Handle: 
- Actual Handle: create_shape
- Geometry Assertion: 
- Reason: expected created_kind=RectShape, actual=

### gui_create_circle_drag_release

- Tool: circle_roi
- Status: failed
- Expected Handle: 
- Actual Handle: create_shape
- Geometry Assertion: 
- Reason: expected created_kind=CircleShape, actual=

### gui_create_ellipse_drag_release

- Tool: ellipse_manual
- Status: failed
- Expected Handle: 
- Actual Handle: create_shape
- Geometry Assertion: 
- Reason: expected created_kind=EllipseShape, actual=

### gui_drag_existing_line_center

- Tool: scan_line
- Status: not_consumed
- Expected Handle: 
- Actual Handle: pointer_pan
- Geometry Assertion: 
- Reason: expected phase=drag_existing, actual=pointer_pan

### gui_drag_existing_circle_radius

- Tool: circle_roi
- Status: not_consumed
- Expected Handle: 
- Actual Handle: pointer_pan
- Geometry Assertion: 
- Reason: expected phase=drag_existing, actual=pointer_pan

### gui_select_existing_shape

- Tool: scan_line
- Status: not_consumed
- Expected Handle: 
- Actual Handle: pointer_pan
- Geometry Assertion: 
- Reason: expected phase=select_existing, actual=pointer_pan

### gui_pointer_pan_no_create

- Tool: 
- Status: not_consumed
- Expected Handle: 
- Actual Handle: pointer_pan
- Geometry Assertion: 
- Reason: 

### gui_create_too_small_rejected

- Tool: scan_line
- Status: failed
- Expected Handle: 
- Actual Handle: create_shape
- Geometry Assertion: 
- Reason: expected status=draft_too_small, actual=failed

