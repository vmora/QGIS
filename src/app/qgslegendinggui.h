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

#include <QWidget>

class QgsVectorLayer;

class APP_EXPORT QgsLegendingGui : public QWidget, private Ui::QgsLegendingGuiBase
{
    Q_OBJECT
  public:
    QgsLegendingGui( QgsVectorLayer* layer, QWidget* parent );
  private:
    QgsVectorLayer * mLayer;

  protected slots:
    void on_checkBox_toggled( bool checked);
};

#endif

