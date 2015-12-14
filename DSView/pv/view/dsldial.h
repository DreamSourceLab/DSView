#ifndef DSVIEW_PV_VIEW_DSLDIAL_H
#define DSVIEW_PV_VIEW_DSLDIAL_H

#include <QRect>
#include <QPainter>

namespace pv {
namespace view {

class dslDial
{
public:
    dslDial(const uint64_t div, const uint64_t step,
            const QVector<uint64_t> value, const QVector<QString> unit);
    virtual ~dslDial();

public:
    /**
     * Paints the dial with a QPainter
     * @param p the QPainter to paint into.
     * @param dialRect the rectangle to draw the dial at.
     **/
    void paint(QPainter &p, QRectF dialRect, QColor dialColor, bool hover, bool inc);

    // set/get current select
    void set_sel(uint64_t sel);
    uint64_t get_sel();

    // boundary detection
    bool isMin();
    bool isMax();

    // get current value
    uint64_t get_value();
    bool set_value(uint64_t value);

    // set/get factor
    void set_factor(uint64_t factor);
    uint64_t get_factor();

private:
    uint64_t _div;
    uint64_t _step;
    QVector<uint64_t> _value;
    QVector<QString> _unit;
    uint64_t _sel;
    uint64_t _factor;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_DSLDIAL_H
