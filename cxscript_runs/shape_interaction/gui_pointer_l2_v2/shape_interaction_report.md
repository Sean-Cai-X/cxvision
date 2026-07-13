# Shape Interaction Test Report

## Summary

- Total Cases: 10
- Pass: 9
- Fail: 1
- Pending Writeback: 0
- Missing Tools: 1

## Case Details

### gui_create_point

- Tool: point_pick
- Status: created
- Expected Handle: 
- Actual Handle: create_shape
- Geometry Assertion: 
- Reason: point created

### gui_create_line_drag_release

- Tool: scan_line
- Status: created
- Expected Handle: 
- Actual Handle: create_shape
- Geometry Assertion: 
- Reason: shape created from annotation tool

### gui_create_rect_drag_release

- Tool: roi_rect
- Status: created
- Expected Handle: 
- Actual Handle: create_shape
- Geometry Assertion: 
- Reason: shape created from annotation tool

### gui_create_circle_drag_release

- Tool: circle_roi
- Status: created
- Expected Handle: 
- Actual Handle: create_shape
- Geometry Assertion: 
- Reason: shape created from annotation tool

### gui_create_ellipse_drag_release

- Tool: ellipse_manual
- Status: created
- Expected Handle: 
- Actual Handle: create_shape
- Geometry Assertion: 
- Reason: shape created from annotation tool

### gui_drag_existing_line_center

- Tool: scan_line
- Status: committed
- Expected Handle: 
- Actual Handle: drag_existing
- Geometry Assertion: 
- Reason: drag committed

### gui_drag_existing_circle_radius

- Tool: circle_roi
- Status: committed
- Expected Handle: 
- Actual Handle: drag_existing
- Geometry Assertion: 
- Reason: drag committed

### gui_select_existing_shape

- Tool: scan_line
- Status: selected
- Expected Handle: 
- Actual Handle: select_existing
- Geometry Assertion: 
- Reason: shape selected and drag started

### gui_pointer_pan_no_create

- Tool: 
- Status: FAIL
- Expected Handle: 
- Actual Handle: 
- Geometry Assertion: 
- Reason: tool not registered: 

### gui_create_too_small_rejected

- Tool: scan_line
- Status: draft_too_small
- Expected Handle: 
- Actual Handle: 
- Geometry Assertion: 
- Reason: line length too small

