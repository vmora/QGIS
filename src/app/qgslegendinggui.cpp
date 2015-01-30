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

#include <QApplication>
#include <QSettings>
#include <QSet>
#include <QPixmap>


QgsCustomVectorLayerLegend::QgsCustomVectorLayerLegend( QgsVectorLayer* vl )
  : mLayer( vl )
{
  connect( mLayer, SIGNAL( rendererChanged() ), this, SIGNAL( itemsChanged() ) );
};

QList<QgsLayerTreeModelLegendNode*> QgsCustomVectorLayerLegend::createLayerTreeModelLegendNodes( QgsLayerTreeLayer* nodeLayer )
{
  QList<QgsLayerTreeModelLegendNode*> nodes;

  QgsFeatureRendererV2* r = mLayer->rendererV2();
  if ( !r )
    return nodes;

  if ( nodeLayer->customProperty( "showFeatureCount", 0 ).toBool() )
    mLayer->countSymbolFeatures();

  QSettings settings;
  if ( settings.value( "/qgis/showLegendClassifiers", false ).toBool() && !r->legendClassificationAttribute().isEmpty() )
    nodes.append( new QgsSimpleLegendNode( nodeLayer, r->legendClassificationAttribute() ) );

  for( auto data: mLegendNodesData)//r->legendSymbolItemsV2() )
    nodes.append( new QgsCustomLegendNode( nodeLayer, QgsLegendSymbolItemV2( data.mSymbol.data(), data.mText, 0 ), data.mFeature, mLayer->pendingFields(), QSize(32,32) ) );

  if ( nodes.count() == 1 && nodes[0]->data( Qt::EditRole ).toString().isEmpty() )
    nodes[0]->setEmbeddedInParent( true );

  return nodes;
}

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
    for ( auto data: customLegend->legendNodesData() )
    {
      QgsCustomLegendNode node( mLayerTreeLayer, QgsLegendSymbolItemV2( data.mSymbol.data(), data.mText, 0 ), data.mFeature, mLayer->pendingFields(), QSize(32,32) );
      QScopedPointer< QStandardItem > item( new QStandardItem( node.data(Qt::DecorationRole).value<QPixmap>(), data.mText ) );
      item->setData( mDataList.size() );
      mDataList.append( data );
      mTargetList.insertRow( 0, item.take() );
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
  connect( &mTargetList, SIGNAL( dataChanged(const QModelIndex &, const QModelIndex &) ), this, SLOT( updateCustomLegend() ) );
  //connect( &mTargetList, SIGNAL( itemChanged(QStandardItem *) ), this, SLOT( updateCustomLegend() ) );
  //connect( &mTargetList, SIGNAL( rowsInserted( const QModelIndex &, int, int ) ), this, SLOT( updateCustomLegend() ) );
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
  QgsFeatureRendererV2* r = mLayer->rendererV2();
  if ( !r )
    return;

  // because we cannot evaluate expressions that are returned by the symbollayer
  // we need to recreate en prepare them
  QMap< QString , QSharedPointer<QgsExpression> > expressionMap;
  for ( auto symbol: r->symbols() )
  {
    if ( symbol )
    {
      for ( auto symbolLayer : symbol->symbolLayers() ) 
      {
        for ( auto prop : symbolLayer->dataDefinedProperties() )
        {
          const QString expStr = symbolLayer->dataDefinedPropertyString( prop );
          QSharedPointer<QgsExpression > exp( new QgsExpression( expStr ) );
          exp->prepare( mLayer->pendingFields() );
          expressionMap[ expStr ] = exp;
        }
      }
    }
  }

  // for all features, evaluate dd props to create a minimum
  // set of features with all possible variations
  QMap< QString , QgsFeature > propIdMap;
  {
    QgsFeatureIterator fit = mLayer->getFeatures();
    QgsFeature feature;
    while ( fit.nextFeature( feature ) )
    {
      for ( auto symbol: r->symbols() )
      {
        if ( symbol )
        {
          QString key;
          for ( auto symbolLayer : symbol->symbolLayers() ) 
          {
            for ( auto prop : symbolLayer->dataDefinedProperties() )
            {
              const QString expStr = symbolLayer->dataDefinedPropertyString( prop );
              const QVariant val = expressionMap[ expStr ]->evaluate( &feature );
              bool isDouble;
              val.toDouble( &isDouble );
              key += prop + "="+(isDouble ? QString::number( val.toDouble(), 'g', 2 ) : val.toString())+" ";
            }
          }
          propIdMap[ key ] = feature;
        }
      }
    }
  }

  // render all features
  mSourceList.clear();
  for ( QMapIterator<QString, QgsFeature> it( propIdMap ); it.hasNext(); ) 
  {
    it.next();
    for ( auto symbol: r->symbols() )
    {
      QgsCustomLegendNode node( mLayerTreeLayer, QgsLegendSymbolItemV2( symbol, it.key(), 0 ), it.value(), mLayer->pendingFields(), QSize(32,32) );
      QScopedPointer< QStandardItem > item( new QStandardItem( node.data(Qt::DecorationRole).value<QPixmap>(), it.key() ) );
      item->setData( mDataList.size() );
      mDataList.append( { QSharedPointer<QgsSymbolV2>(symbol->clone()), it.value(), ""} );
      mSourceList.insertRow( 0, item.take() );
    }
  }
}

void QgsLegendingGui::updateCustomLegend()
{
  QList< QgsCustomVectorLayerLegend::NodeData > legendNodesData;
  for ( int r = 0; r < mTargetList.rowCount(); r++ )
  {
    const QStandardItem * item = mTargetList.item( r );
    QgsCustomVectorLayerLegend::NodeData data( mDataList[ item->data().toInt() ] );
    data.mText = item->text();
    legendNodesData.append( data );
  }
  static_cast<QgsCustomVectorLayerLegend *>( mLayer->legend() )->setLegendNodesData( legendNodesData );
}

