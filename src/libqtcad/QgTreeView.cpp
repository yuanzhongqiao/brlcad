/*                    Q G T R E E V I E W . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file QgTreeView.cpp
 *
 * BRL-CAD .g tree visualization widget.
 *
 * TODO - look at https://doc.qt.io/qt-5/qtwidgets-itemviews-customsortfiltermodel-example.html
 * to add filtering abilities to the tree view.  Particularly on large models, a flat listing
 * of objects is going to be too unwieldy to want to work with much - we need a way to trim it
 * down for a specific task.
 */

#include "common.h"
#include <QPainter>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QUrl>
#include "qtcad/QgModel.h"
#include "qtcad/QgTreeSelectionModel.h"
#include "qtcad/QgTreeView.h"
#include "qtcad/SignalFlags.h"

void gObjDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    int aflag = 0;
    if (!cadtreeview)
	return;

    int sflag = index.data(QgModel::SelectDisplayRole).toInt();
    if (sflag == 1) {
	painter->fillRect(option.rect, option.palette.highlight());
	goto text_string;
    }

    aflag = index.data(QgModel::HighlightDisplayRole).toInt();
    if (!cadtreeview->isExpanded(index) && aflag == 1) {
	painter->fillRect(option.rect, QBrush(QColor(220, 200, 30)));
	goto text_string;
    }
    if (aflag == 2) {
	painter->fillRect(option.rect, QBrush(QColor(10, 10, 50)));
	goto text_string;
    }
    if (aflag == 3) {
	painter->fillRect(option.rect, QBrush(QColor(10, 10, 150)));
	goto text_string;
    }


text_string:
    QString text = index.data().toString();
    int bool_op = index.data(QgModel::BoolInternalRole).toInt();
    switch (bool_op) {
	case DB_OP_INTERSECT:
	    text.prepend("  + ");
	    break;
	case DB_OP_SUBTRACT:
	    text.prepend("  - ");
	    break;
    }
    text.prepend(" ");

    // rect with width proportional to value
    //QRect rect(option.rect.adjusted(4,4,-4,-4));

    // draw the value bar
    //painter->fillRect(rect, QBrush(QColor("steelblue")));
    //
    // draw text label

    QImage type_icon = index.data(QgModel::TypeIconDisplayRole).value<QImage>().scaledToHeight(option.rect.height()-2);
    QRect image_rect = type_icon.rect();
    image_rect.moveTo(option.rect.topLeft());
    image_rect.translate(0, 1);
    painter->drawImage(image_rect, type_icon);

    QRect text_rect(type_icon.rect().topRight(), option.rect.bottomRight());
#if 0
    QFont f = option.font;
    f.setItalic(true);
    painter->setFont(f);
#endif
    QPen tpen = painter->pen();
    int drawn_state = index.data(QgModel::DrawnDisplayRole).toInt();
    if (drawn_state == 1) {
	if (sflag) {
	    painter->setPen(QColor(0, 70, 0));
	} else {
	    painter->setPen(QColor(0, 200, 0));
	}
    }
    if (drawn_state == 2)
	painter->setPen(QColor(150, 70, 200));
    text_rect.moveTo(image_rect.topRight());
    painter->drawText(text_rect, text, QTextOption(Qt::AlignLeft));
    painter->setPen(tpen);
}

QSize gObjDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize name_size = option.fontMetrics.size(Qt::TextSingleLine, index.data().toString());
    QSize bool_size;
    int bool_op = index.data(QgModel::BoolInternalRole).toInt();
    switch (bool_op) {
	case DB_OP_UNION:
	    bool_size = option.fontMetrics.size(Qt::TextSingleLine, " ");
	    break;
	case DB_OP_INTERSECT:
	    bool_size = option.fontMetrics.size(Qt::TextSingleLine, "   + ");
	    break;
	case DB_OP_SUBTRACT:
	    bool_size = option.fontMetrics.size(Qt::TextSingleLine, "   - ");
	    break;
    }
    int item_width = name_size.rwidth() + bool_size.rwidth() + 18;
    int item_height = name_size.rheight();
    return QSize(item_width, item_height);
}


QgTreeView::QgTreeView(QWidget *pparent, QgModel *treemodel) : QTreeView(pparent)
{
    this->setContextMenuPolicy(Qt::CustomContextMenu);

    // Optimization: https://stackoverflow.com/a/31356930/2037687
    this->setUniformRowHeights(true);

    // We use double click to control drawing - don't want the tree to expand
    // and contract at the same time.  To open or close combs, use arrows to
    // the left of the text and icon.
    setExpandsOnDoubleClick(false);

    // Set model
    m = treemodel;
    setModel(treemodel);

    // Set custom selection model
    QgTreeSelectionModel *sm = new QgTreeSelectionModel(treemodel, this, this);
    this->setSelectionModel(sm);
    // Allow for multiple selection
    this->setSelectionMode(ExtendedSelection);

    setItemDelegate(new gObjDelegate(this));

    header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header()->setStretchLastSection(true);
    QObject::connect(this, &QgTreeView::expanded, this, &QgTreeView::tree_column_size);
    QObject::connect(this, &QgTreeView::collapsed, this, &QgTreeView::tree_column_size);
    QObject::connect(this, &QgTreeView::clicked, sm, &QgTreeSelectionModel::update_selected_node_relationships);
    QObject::connect(this, &QgTreeView::customContextMenuRequested, (QgTreeView *)this, &QgTreeView::context_menu);
    QObject::connect(this, &QgTreeView::doubleClicked, (QgTreeView *)this, &QgTreeView::do_draw_toggle);
}

void
QgTreeView::drawBranches(QPainter* painter, const QRect& rrect, const QModelIndex& index) const
{
    QModelIndex selected_idx = ((QgTreeView *)this)->selected();
    if (!(index == selected_idx)) {
	int aflag = index.data(QgModel::HighlightDisplayRole).toInt();
	if (!(QgTreeView *)this->isExpanded(index) && aflag == 1) {
	    painter->fillRect(rrect, QBrush(QColor(220, 200, 30)));
	}
	if (aflag == 2) {
	    painter->fillRect(rrect, QBrush(QColor(10, 10, 50)));
	}
	if (aflag == 3) {
	    painter->fillRect(rrect, QBrush(QColor(10, 10, 150)));
	}
    }
    QTreeView::drawBranches(painter, rrect, index);
}

void QgTreeView::header_state()
{
    //header()->setStretchLastSection(true);
    //int header_length = header()->length();
    //header()->setSectionResizeMode(0, QHeaderView::Fixed);
    //resizeColumnToContents(0);
    //int column_width = columnWidth(0);

    // TODO - when the dock widget is resized, there is a missing
    // update somewhere - the scrollbar doesn't appear until the tree
    // is selected...

    header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    header()->setStretchLastSection(false);
    if (header()->count() == 1) {
	header()->setStretchLastSection(false);
    } else {
	header()->setStretchLastSection(true);
    }
    header()->setMaximumHeight(1);
}

void QgTreeView::resizeEvent(QResizeEvent *)
{
    header_state();
    emit m->layoutChanged();
}

void QgTreeView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::RightButton && e->type() == QEvent::MouseButtonPress) {
	// We want the context menu, but NOT the selection
	// behavior
	e->accept();
	return;
    }

    // Otherwise, do the normal event handling
    QTreeView::mousePressEvent(e);
}

void QgTreeView::tree_column_size(const QModelIndex &)
{
    header_state();
}

void QgTreeView::context_menu(const QPoint &point)
{
    QModelIndex index = indexAt(point);
    QgItem *cnode = static_cast<QgItem *>(index.internalPointer());


    QAction* draw_action = new QAction("Draw", NULL);
    // https://stackoverflow.com/a/28647342/2037687
    QVariant draw_action_v;
#if QT_VERSION < QT_VERSION_CHECK(5, 12, 0)
    draw_action_v = qVariantFromValue((void *)cnode);
#else
    draw_action_v = QVariant::fromValue((void *)cnode);
#endif
    draw_action->setData(draw_action_v);
    connect(draw_action, &QAction::triggered, m, &QgModel::draw_action);


    QAction* erase_action = new QAction("Erase", NULL);
    QVariant erase_action_v;
#if QT_VERSION < QT_VERSION_CHECK(5, 12, 0)
    erase_action_v = qVariantFromValue((void *)cnode);
#else
    erase_action_v = QVariant::fromValue((void *)cnode);
#endif
    erase_action->setData(erase_action_v);
    connect(erase_action, &QAction::triggered, m, &QgModel::erase_action);


    QMenu *menu = new QMenu("Object Actions", NULL);
    menu->addAction(draw_action);
    menu->addAction(erase_action);
    menu->exec(mapToGlobal(point));
    delete menu;
}

void QgTreeView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    if (selected.indexes().size()) {
	// Stash a valid selection for potential post-model rebuild restoration.
	QModelIndex nindex = selected.indexes().first();
	if (nindex.isValid())
	    cached_selection_idx = nindex;
    }
    QTreeView::selectionChanged(selected, deselected);
}

QModelIndex QgTreeView::selected()
{
    if (selectedIndexes().count() == 1) {
	return selectedIndexes().first();
    } else {
	return QModelIndex();
    }
}

void
QgTreeView::do_draw_toggle(const QModelIndex &index)
{
    QgItem *cnode = static_cast<QgItem *>(index.internalPointer());

    if (!m->gedp)
	return;

    struct bview *v = m->gedp->ged_gvp;
    if (!v)
	return;

    struct db_full_path *clicked_path = cnode->fp();

    bool do_draw = true;
    struct bu_ptbl *sg = bv_view_objs(m->gedp->ged_gvp, BV_DB_OBJS);
    for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
	struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
	struct db_full_path currently_drawn;
	db_full_path_init(&currently_drawn);
	db_string_to_path(&currently_drawn, m->gedp->dbip, bu_vls_cstr(&cg->s_name));
	if (db_full_path_match_top(&currently_drawn, clicked_path)) {
	    do_draw = false;
	    break;
	}
    }
    db_free_full_path(clicked_path);
    BU_PUT(clicked_path, struct db_full_path);

    QString cnode_path = cnode->toString();
    if (do_draw) {
	m->draw(cnode_path);
    } else {
	m->erase(cnode_path);
    }

}

void
QgTreeView::redo_expansions(void *)
{
    std::unordered_set<QgItem *>::iterator i_it;
    for (i_it = m->items->begin(); i_it != m->items->end(); i_it++) {
	QgItem *itm = *i_it;
	QModelIndex idx = m->NodeIndex(itm);
	if (itm->open_itm && !isExpanded(idx)) {
	    setExpanded(idx, true);
	}
    }
}

void
QgTreeView::redo_highlights()
{
    // Restore the previous selection, if we have no replacement and its still valid
    QModelIndex selected_idx = selected();
    if (!selected_idx.isValid()) {
	QgItem *cnode = static_cast<QgItem *>(cached_selection_idx.internalPointer());
	if (m->items->find(cnode) != m->items->end()) {
	    selected_idx = cached_selection_idx;
	    selectionModel()->select(selected_idx, QItemSelectionModel::Select | QItemSelectionModel::Rows);
	} else {
	    cached_selection_idx = QModelIndex();
	}
    }

    QgTreeSelectionModel *selm = (QgTreeSelectionModel *)selectionModel();
    selm->update_selected_node_relationships(selected_idx);
}

void QgTreeView::expand_path(QString path)
{
    int i = 0;
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    QStringList path_items = path.split("/", QString::SkipEmptyParts);
#else
    QStringList path_items = path.split("/", Qt::SkipEmptyParts);
#endif
    QgItem *r = m->root();
    while (i < path_items.size()) {

	for (int j = 0; j < r->childCount(); j++) {
	    QgItem *c = r->child(j);
	    gInstance *g = c->instance();
	    if (!g)
		continue;
	    if (QString::fromStdString(g->dp_name) != path_items.at(i)) {
		continue;
	    }
	    QModelIndex path_component = m->NodeIndex(c);
	    if (i == path_items.size() - 1) {
		selectionModel()->clearSelection();
		selectionModel()->select(path_component, QItemSelectionModel::Select | QItemSelectionModel::Rows);
		emit (clicked(path_component));
	    } else {
		expand(path_component);
		r = c;
	    }
	    break;
	}
	i++;
    }
}

void QgTreeView::expand_link(const QUrl &link)
{
    expand_path(link.path());
}


void QgTreeView::qgitem_select_sync(QgItem *itm)
{
    struct ged *gedp = m->gedp;
    struct ged_selection_set *gs = NULL;
    if (gedp->ged_selection_sets) {
	struct bu_ptbl ssets = BU_PTBL_INIT_ZERO;
	size_t scnt = ged_selection_sets_lookup(&ssets, gedp->ged_selection_sets, "default");
	if (scnt == 1)
	    gs = (struct ged_selection_set *)BU_PTBL_GET(&ssets, 0);
	bu_ptbl_free(&ssets);
    }
    if (!gs)
	return;

    QgTreeSelectionModel *selm = (QgTreeSelectionModel *)selectionModel();
    selm->ged_selection_sync(itm, gs);
    selm->ged_drawn_sync(itm, gedp);
}

void QgTreeView::do_view_update(unsigned long long flags)
{
    struct ged *gedp = m->gedp;
    QgTreeSelectionModel *selm = (QgTreeSelectionModel *)selectionModel();

    if (flags & QTCAD_VIEW_SELECT && gedp->ged_cset)
	selm->ged_selection_sync(NULL, gedp->ged_cset);

    if (flags & QTCAD_VIEW_DRAWN)
	selm->ged_drawn_sync(NULL, gedp);

    // TODO - can the mode logic be triggered from here as well?

    emit m->layoutChanged();
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

