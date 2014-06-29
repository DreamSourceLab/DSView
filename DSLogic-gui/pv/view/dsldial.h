#ifndef DSLOGIC_PV_VIEW_DSLDIAL_H
#define DSLOGIC_PV_VIEW_DSLDIAL_H

#include <QRect>
#include <QPainter>

namespace pv {
namespace view {

class dslDial
{
public:
    dslDial(quint64 div, quint64 step,
            QVector<quint64> value, QVector<QString> unit);
    virtual ~dslDial();

public:
    /**
     * Paints the dial with a QPainter
     * @param p the QPainter to paint into.
     * @param dialRect the rectangle to draw the dial at.
     **/
    void paint(QPainter &p, QRectF dialRect, QColor dialColor);

    // set/get current select
    void set_sel(quint64 sel);
    quint64 get_sel();

    // boundary detection
    bool isMin();
    bool isMax();

    // get current value
    quint64 get_value();
    bool set_value(quint64 value);

private:
    quint64 _div;
    quint64 _step;
    QVector<quint64> _value;
    QVector<QString> _unit;
    quint64 _sel;
};

} // namespace view
} // namespace pv

#endif // DSLOGIC_PV_VIEW_DSLDIAL_H
