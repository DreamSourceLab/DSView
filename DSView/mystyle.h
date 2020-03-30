#ifndef MYSTYLE_H
#define MYSTYLE_H

#include <QProxyStyle>

class MyStyle : public QProxyStyle
{
    Q_OBJECT
    public:
    int pixelMetric(PixelMetric metric, const QStyleOption * option = 0, const QWidget * widget = 0 ) const {
        int s = QProxyStyle::pixelMetric(metric, option, widget);
        if (metric == QStyle::PM_SmallIconSize) {
            s = 24;
        }
        return s;
    }
};

#endif // MYSTYLE_H
