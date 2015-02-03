/***************************************************************************
  qgslegendinggui.h
  User-defined legend for vector layers
  -------------------
         begin                : January 2014
         copyright            : (C) Vincent Mora
         email                : vincent dot mora at oslandia dot com

 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QgsLegendingGUI_H
#define QgsLegendingGUI_H

#include <ui_qgslegendingguibase.h>

#include "qgslayertreelayer.h"
#include "qgslayertreegroup.h"
#include "qgslayertreemodel.h"
#include "qgslayertreemodellegendnode.h"
#include "qgsmaplayerlegend.h"
#include "qgsfeaturerequest.h"

#include <QWidget>
#include <QScopedPointer>
#include <QStandardItemModel>

class QgsVectorLayer;

// The legend is saved in QgsCustomVectorLayerLegend 
// because if it were saved here, we would loose it on
// destruction
class APP_EXPORT QgsLegendingGui : public QWidget, private Ui::QgsLegendingGuiBase
{
    Q_OBJECT
  public:
    QgsLegendingGui( QgsVectorLayer* layer, QWidget* parent );
  private:
    QgsVectorLayer* mLayer;
    QgsLayerTreeLayer* mLayerTreeLayer;
    QgsLayerTreeGroup mRoot;
    QgsLayerTreeModel mModel;

    QStandardItemModel mSourceList;
    QStandardItemModel mTargetList;

    void populateSourceList( const QgsFeatureRequest & request = QgsFeatureRequest() );

  protected slots:
    void on_checkBox_toggled( bool checked);
    void on_symbolsInLayerButton_clicked();
    void on_symbolsInExtendButton_clicked();
    void updateCustomLegend();

};

#endif

