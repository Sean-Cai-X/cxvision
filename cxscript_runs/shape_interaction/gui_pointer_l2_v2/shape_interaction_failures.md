# Shape Interaction Failures

## gui_create_point

- Tool: point_pick
- Expected Handle: 
- Actual Handle: create_shape
- Geometry Assertion: 
- Status: created
- Reason: expected created_kind=PointsShape, actual=Points

## gui_create_line_drag_release

- Tool: scan_line
- Expected Handle: 
- Actual Handle: create_shape
- Geometry Assertion: 
- Status: failed
- Reason: expected created_kind=LineShape, actual=

## gui_create_rect_drag_release

- Tool: roi_rect
- Expected Handle: 
- Actual Handle: create_shape
- Geometry Assertion: 
- Status: failed
- Reason: expected created_kind=RectShape, actual=

## gui_create_circle_drag_release

- Tool: circle_roi
- Expected Handle: 
- Actual Handle: create_shape
- Geometry Assertion: 
- Status: failed
- Reason: expected created_kind=CircleShape, actual=

## gui_create_ellipse_drag_release

- Tool: ellipse_manual
- Expected Handle: 
- Actual Handle: create_shape
- Geometry Assertion: 
- Status: failed
- Reason: expected created_kind=EllipseShape, actual=

## gui_drag_existing_line_center

- Tool: scan_line
- Expected Handle: 
- Actual Handle: pointer_pan
- Geometry Assertion: 
- Status: not_consumed
- Reason: expected phase=drag_existing, actual=pointer_pan

## gui_drag_existing_circle_radius

- Tool: circle_roi
- Expected Handle: 
- Actual Handle: pointer_pan
- Geometry Assertion: 
- Status: not_consumed
- Reason: expected phase=drag_existing, actual=pointer_pan

## gui_select_existing_shape

- Tool: scan_line
- Expected Handle: 
- Actual Handle: pointer_pan
- Geometry Assertion: 
- Status: not_consumed
- Reason: expected phase=select_existing, actual=pointer_pan

## gui_create_too_small_rejected

- Tool: scan_line
- Expected Handle: 
- Actual Handle: create_shape
- Geometry Assertion: 
- Status: failed
- Reason: expected status=draft_too_small, actual=failed

