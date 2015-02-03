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
#include "qgsrendererv2.h"
#include "qgslayertreemodellegendnode.h"
#include "qgslayertreemodel.h"
#include "qgslayertreegroup.h"
#include "qgslegendsymbolitemv2.h"
#include "qgssymbolv2.h"
#include "qgssymbollayerv2.h"
#include "qgisapp.h"
#include "qgsmapcanvas.h"

#include <QApplication>
#include <QSettings>
#include <QSet>
#include <QPixmap>



QgsLegendingGui::QgsLegendingGui( QgsVectorLayer* layer, QWidget* parent )
    : QWidget( parent )
    , mLayer( layer )
    , mModel( &mRoot, this )
{
  setupUi( this );

  if ( ! dynamic_cast<QgsCustomVectorLayerLegend *>( mLayer->legend() ) 
      && ! dynamic_cast<QgsDefaultVectorLayerLegend *>( mLayer->legend() ) )
    this->setEnabled( false ); // don't deal with plugin layers yet

  mLayerTreeLayer = new QgsLayerTreeLayer( mLayer ) ;
  mRoot.addChildNode( mLayerTreeLayer ); // takes ownership
  mModel.refreshLayerLegend( mLayerTreeLayer );

  treeView->setModel( &mModel );
  treeView->setHeaderHidden( true );
  treeView->expandAll();

  QgsCustomVectorLayerLegend * customLegend = dynamic_cast<QgsCustomVectorLayerLegend *>( mLayer->legend() );
  if ( customLegend  )
  {
    // populate the list with the legend contend
    for ( auto legendItem: customLegend->legendItems() )
    {
      QgsSymbolV2LegendNode node( mLayerTreeLayer, QgsLegendSymbolItemV2( legendItem.symbol(), legendItem.label(), 0 ) );
      QScopedPointer< QStandardItem > item( new QStandardItem( node.data(Qt::DecorationRole).value<QPixmap>(), legendItem.label() ) );
      QDomDocument doc;
      QDomElement elem = QgsSymbolLayerV2Utils::saveSymbol( legendItem.label(), legendItem.symbol(), doc );
      doc.appendChild( elem );
      item->setData( doc.toString() );
      mTargetList.appendRow( item.take() );
    }
    updateCustomLegend();
  }
  checkBox->setCheckState( customLegend ? Qt::Checked : Qt::Unchecked );
  frame->setEnabled( customLegend );

  sourceListView->setModel( &mSourceList );
  targetListView->setModel( &mTargetList );

  sourceListView->setDragDropMode( QAbstractItemView::DragDrop );
  sourceListView->setDefaultDropAction( Qt::MoveAction );
  targetListView->setDragDropMode( QAbstractItemView::DragDrop );
  targetListView->setDefaultDropAction( Qt::MoveAction );

  sourceListView->setIconSize( QSize(32,32) );
  targetListView->setIconSize( QSize(32,32) );
  
  connect( checkBox, SIGNAL( toggled( bool ) ), this, SLOT( on_checkBox_toggled( bool ) ) );
  
  connect( symbolsInLayerButton, SIGNAL( clicked() ), this, SLOT( on_symbolsInLayerButton_clicked() ) );
  connect( symbolsInExtendButton, SIGNAL( clicked() ), this, SLOT( on_symbolsInExtendButton_clicked() ) );


  connect( &mTargetList, SIGNAL( dataChanged(const QModelIndex &, const QModelIndex &) ), this, SLOT( updateCustomLegend() ) );
  connect( &mTargetList, SIGNAL( rowsMoved( const QModelIndex &, int, int, const QModelIndex &, int ) ), this, SLOT( updateCustomLegend() ) );
  connect( &mTargetList, SIGNAL( rowsRemoved( const QModelIndex &, int, int) ), this, SLOT( updateCustomLegend() ) );
}

void QgsLegendingGui::on_checkBox_toggled( bool checked )
{
  frame->setEnabled( checked );

  //@todo clone the existing legend or allow QgsMapLayer 
  //      to give up ownership once a lot of legend types exist
  mLayer->setLegend( checked 
      ? static_cast<QgsMapLayerLegend*>( new QgsCustomVectorLayerLegend( mLayer ) )
      : static_cast<QgsMapLayerLegend*>( new QgsDefaultVectorLayerLegend( mLayer ) ) );
  if ( checked ) updateCustomLegend();
}

void QgsLegendingGui::on_symbolsInLayerButton_clicked()
{
  populateSourceList();
}

void QgsLegendingGui::on_symbolsInExtendButton_clicked()
{
  populateSourceList( QgsFeatureRequest( QgisApp::instance()->mapCanvas()->extent() ) );
}
  
void QgsLegendingGui::populateSourceList( const QgsFeatureRequest & request )
{
  QgsFeatureRendererV2* r = mLayer->rendererV2();
  if ( !r )
    return;
  const QgsSymbolV2List symbolList( r->symbols() );

  // evaluate expressions for all features
  typedef QMap< QgsFeatureId, QString > ExpressionValues;
  typedef QMap< QString, ExpressionValues > SymbolLayerProps;
  typedef QList< SymbolLayerProps > SymbolLayers;
  typedef QList< SymbolLayers > Symbols;
  Symbols symbols;
  QList< QgsFeatureId > featureIds;
  for ( auto symbol: symbolList )
  {
    SymbolLayers symLayers;
    if ( symbol )
    {
      for ( auto symbolLayer : symbol->symbolLayers() ) 
      {
        SymbolLayerProps symLayerProps;
        for ( auto prop : symbolLayer->dataDefinedProperties() )
        {
          QgsExpression exp( symbolLayer->dataDefinedPropertyString( prop ) );
          exp.prepare( mLayer->pendingFields() );

          QgsFeatureIterator fit = mLayer->getFeatures(request);
          QgsFeature feature;

          ExpressionValues exprValues;
          while ( fit.nextFeature( feature ) )
          {
            QVariant val = exp.evaluate( &feature ); 
            // round double to 2 significant digits
            // @todo round angles to 5 degrees
            bool isDouble;
            val.toDouble( &isDouble );
            if ( isDouble ) val =  QString::number( val.toDouble(), 'g', 2 );
            exprValues[ feature.id() ] = val.toString();
          }
          symLayerProps[ prop ] = exprValues;
        }
        symLayers.append( symLayerProps );
      }
    }
    symbols.append( symLayers );
  }
  
  // for each symbol, we create a string of ddprop values and use
  // it as a key in a set, if the key is already in, we skip
  // if it's a new key, we create a new entry in the list
  for ( int s = 0; s < symbolList.size(); s++ )
  {
    QSet< QString > distinctValues;
    for ( auto fid : mLayer->allFeatureIds() )
    {
      QString key;
      for ( int l = 0; l < symbols[s].size(); l++ )
      {
        if ( symbols[s].size() > 1 ) key += "Symbol "+QString::number( l + 1 )+" ";
        for ( auto prop: symbols[s][l].keys() )
          key += prop + "=" + symbols[s][l][prop][fid] + " ";
      }
      if ( !distinctValues.contains( key ) )
      {
        distinctValues.insert( key );
        // create an entry in the list
        QScopedPointer< QgsSymbolV2 > symbol( symbolList[s]->clone() );
        QgsSymbolLayerV2List symbolLayers( symbol->symbolLayers() );
        for ( int l = 0; l < symbolLayers.size(); l++ )
          for ( auto prop: symbolLayers[l]->dataDefinedProperties() )
            symbolLayers[l]->setDataDefinedProperty( prop, symbols[s][l][prop][fid] );

        QgsSymbolV2LegendNode node( mLayerTreeLayer, QgsLegendSymbolItemV2( symbol.data(), key, 0 ) );
        QScopedPointer< QStandardItem > item( new QStandardItem( node.data(Qt::DecorationRole).value<QPixmap>(), key ) );
        QDomDocument doc;
        QDomElement elem = QgsSymbolLayerV2Utils::saveSymbol( key, symbol.data(), doc );
        doc.appendChild( elem );
        item->setData( doc.toString() );
        mSourceList.appendRow( item.take() );
      }
    }
  }
}

void QgsLegendingGui::updateCustomLegend()
{
  QgsCustomVectorLayerLegend* customLegend = static_cast<QgsCustomVectorLayerLegend *>( mLayer->legend() );
  customLegend->clear();
  for ( int i = 0; i < mTargetList.rowCount(); i++ )
  {
    QDomDocument doc;
    doc.setContent( mTargetList.item(i)->data().toString() );
    QDomElement elem = doc.documentElement();
    QScopedPointer< QgsSymbolV2 > symbol( QgsSymbolLayerV2Utils::loadSymbol( elem ) );
    customLegend->append( QgsLegendSymbolItemV2( symbol.data(), mTargetList.item(i)->text(), 0 ) );
  }
}

