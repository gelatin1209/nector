#include "row_scroll.h"

#include "row_layout.h"

#include <algorithm>
#include <cmath>

namespace interface_internal {

nector_status setRowScroll(nector_interface_row_handle row, double scroll) {
    if (!rowValid(row) || !std::isfinite(scroll)) {
        return interfaceFail(nector_invalid_argument, "row or scroll value is invalid");
    }
    nector_status status = refreshRow(row);
    if (status != nector_ok) return status;
    row->scroll = std::max(0.0, std::min(row->max_scroll, scroll));
    return refreshRow(row);
}

nector_status scrollRowBy(nector_interface_row_handle row, double delta) {
    nector_status status = refreshRow(row);
    if (status != nector_ok) return status;
    double target = row->scroll;
    if (delta > 0 && delta >= row->max_scroll - row->scroll) target = row->max_scroll;
    else if (delta < 0 && -delta >= row->scroll) target = 0;
    else target += delta;
    return setRowScroll(row, target);
}

} // namespace interface_internal
