/*
 *
 * This file is part of Tulip (www.tulip-software.org)
 *
 * Authors: David Auber and the Tulip development Team
 * from LaBRI, University of Bordeaux 1 and Inria Bordeaux - Sud Ouest
 *
 * Tulip is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * Tulip is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 */
///@cond DOXYGEN_HIDDEN


#ifndef COLORSCALECONFIGDIALOG_H_
#define COLORSCALECONFIGDIALOG_H_

#include <tulip/ColorScale.h>

#include <QDialog>

namespace Ui {
class ColorScaleDialog;
}

class QTableWidgetItem;
class QListWidgetItem;
class QLabel;

namespace tlp {

class TLP_QT_SCOPE ColorScaleConfigDialog : public QDialog {

  Q_OBJECT

public :
  ColorScaleConfigDialog(const ColorScale &colorScale = ColorScale(), QWidget *parent = NULL);
  ~ColorScaleConfigDialog();
  void setColorScale(const ColorScale &colorScale);
  ColorScale getColorScale() const;

protected :

  void resizeEvent(QResizeEvent * event);
  void showEvent(QShowEvent * event);

private slots :

  void accept();
  void pressButtonBrowse();
  void nbColorsValueChanged(int value);
  void colorTableItemDoubleClicked(QTableWidgetItem *item);
  void displaySavedGradientPreview();
  void displayUserGradientPreview();
  void saveCurrentColorScale();
  void deleteSavedColorScale();
  void reeditSaveColorScale(QListWidgetItem *savedColorScaleItem);
  void importColorScaleFromImageFile();
  void invertEditedColorScale();

private :
  Ui::ColorScaleDialog *_ui;

  void setColorScaleFromImage(const QString &imageFilePath);

  void loadUserSavedColorScales();
  void displayGradientPreview(const QList<QColor> &colorsVector, bool gradient, QLabel *displayLabel);

  ColorScale colorScale;
  std::string gradientsImageDirectory;

  static std::map<QString, std::vector<Color> > tulipImageColorScales;
  static void loadTulipImageColorScales();
  static std::vector<Color> getColorScaleFromImage(const QString &imageFilePath);
};

}

#endif /* COLORSCALECONFIGDIALOG_H_ */
///@endcond
