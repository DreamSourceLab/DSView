#include "dsldial.h"

#include <assert.h>

namespace pv {
namespace view {

dslDial::dslDial(quint64 div, quint64 step,
                 QVector<quint64> value, QVector<QString> unit)
{
    assert(div > 0);
    assert(step > 0);
    assert((quint64)value.count() == div);
    assert(unit.count() > 0);

    _div = div;
    _step = step;
    _value = value;
    _unit = unit;
    _sel = 0;
}

dslDial::~dslDial()
{
}

void dslDial::paint(QPainter &p, QRectF dialRect, QColor dialColor)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(dialColor);
    p.setBrush(dialColor);

    int dialStartAngle = 225 * 16;
    int dialSpanAngle = -270 * 16;

    // draw dial arc
    p.drawArc(dialRect, dialStartAngle, dialSpanAngle);
    // draw ticks
    p.save();
    p.translate(dialRect.center());
    p.rotate(45);
    for (quint64 i = 0; i < _div; i++) {
        // draw major ticks
        p.drawLine(0, dialRect.width()/2+3, 0, dialRect.width()/2+8);
        // draw minor ticks
        for (quint64 j = 0; (j < 5) && (i < _div - 1); j++) {
            p.drawLine(0, dialRect.width()/2+3, 0, dialRect.width()/2+5);
            p.rotate(54.0/(_div-1));
        }
    }
    // draw pointer
    p.rotate(90+270.0/(_div-1)*_sel);
    p.drawEllipse(-3, -3, 6, 6);
    p.drawLine(3, 0, 0, dialRect.width()/2-3);
    p.drawLine(-3, 0, 0, dialRect.width()/2-3);
    p.restore();
    // draw value
    quint64 displayValue = _value[_sel];
    quint64 displayIndex = 0;
    while(displayValue / _step >= 1) {
        displayValue = displayValue / _step;
        displayIndex++;
    }
    QString pText = QString::number(displayValue) + _unit[displayIndex] + "/div";
    QFontMetrics fm(p.font());
    p.drawText(QRectF(dialRect.left(), dialRect.bottom()-dialRect.width()*0.3+fm.height()*0.5, dialRect.width(), fm.height()), Qt::AlignCenter, pText);

}

void dslDial::set_sel(quint64 sel)
{
    assert(sel < _div);

    _sel = sel;
}

quint64 dslDial::get_sel()
{
    return _sel;
}

bool dslDial::isMin()
{
    if(_sel == 0)
        return true;
    else
        return false;
}

bool dslDial::isMax()
{
    if(_sel == _div - 1)
        return true;
    else
        return false;
}

quint64 dslDial::get_value()
{
    return _value[_sel];
}

bool dslDial::set_value(quint64 value)
{
    assert(_value.contains(value));
    _sel = _value.indexOf(value, 0);
}

} // namespace view
} // namespace pv
