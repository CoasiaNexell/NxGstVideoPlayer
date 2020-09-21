#ifndef PLAYLISTWINDOW_H
#define PLAYLISTWINDOW_H

#include <QDialog>
#include <QListWidgetItem>

#include "NX_CFileList.h"

namespace Ui {
class PlayListWindow;
}

class PlayListWindow : public QDialog
{
	Q_OBJECT

public:
	explicit PlayListWindow(QWidget *parent = 0);
	~PlayListWindow();

public:
	void resizeEvent(QResizeEvent *event);
	void setList(NX_CFileList *pFileList);
	void setCurrentIndex(int idx);
	int getCurrentIndex();

public:
	QModelIndex m_selectIdx;
	int         m_pretIdx;
	int         m_selectCount;

private slots:
	void on_btnCancel_released();
	void on_btnOk_released();
	void on_listWidget_itemClicked(QListWidgetItem *item);

	void SetupUI();

private:
	Ui::PlayListWindow *ui;
};

#endif // PLAYLISTWINDOW_H
