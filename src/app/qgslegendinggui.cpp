/***************************************************************************
  qgslegendinggui.cpp
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

#include "qgslegendinggui.h"
#include "qgsvectorlayer.h"
#include "qgslayertreemodellegendnode.h"

#include <QApplication>

QgsLegendingGui::QgsLegendingGui( QgsVectorLayer* layer, QWidget* parent )
    : QWidget( parent )
    , mLayer( layer )
{
  setupUi( this );
  connect( checkBox, SIGNAL( toggled( bool ) ), this, SLOT( on_checkBox_toggled( bool ) ) );
  //treeView->setModel( mTreeModel );

  //QgsSymbolV2LegendNode
  //mTreeModel
}

void QgsLegendingGui::on_checkBox_toggled( bool checked )
{
  treeView->setEnabled( checked );
}
