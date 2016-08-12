/*
 * This file is part of the DSView project.
 *
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <libsigrokdecode/libsigrokdecode.h>

#include <boost/foreach.hpp>
#include <boost/thread/thread.hpp>

#include <pv/data/decode/annotation.h>
#include <pv/data/decode/rowdata.h>

#include "decoderstack.h"
#include "decodermodel.h"

using namespace boost;
using namespace std;

namespace pv {
namespace data {

DecoderModel::DecoderModel(QObject *parent)
    : QAbstractTableModel(parent),
      _decoder_stack(NULL)
{
}

void DecoderModel::setDecoderStack(boost::shared_ptr<pv::data::DecoderStack> decoder_stack)
{
    beginResetModel();
    _decoder_stack = decoder_stack;
    endResetModel();
}

const boost::shared_ptr<pv::data::DecoderStack>& DecoderModel::getDecoderStack() const
{
    return _decoder_stack;
}

int DecoderModel::rowCount(const QModelIndex & /* parent */) const
{
    if (_decoder_stack)
        return _decoder_stack->list_annotation_size();
    else
        return 100;
}
int DecoderModel::columnCount(const QModelIndex & /* parent */) const
{
    if (_decoder_stack)
        return _decoder_stack->list_rows_size();
    else
        return 1;
}

QVariant DecoderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == Qt::TextAlignmentRole) {
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    } else if (role == Qt::DisplayRole) {
        if (_decoder_stack) {
            pv::data::decode::Annotation ann;
            if (_decoder_stack->list_annotation(ann, index.column(), index.row())) {
                return ann.annotations().at(0);
            }
        }
    }
    return QVariant();
}

QVariant DecoderModel::headerData(int section,
                                   Qt::Orientation  orientation,
                                   int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Vertical)
        return section;

    if (_decoder_stack) {
        QString title;
        if (_decoder_stack->list_row_title(section, title))
            return title;
    }
    return QVariant();
}

} // namespace data
} // namespace pv
