#ifndef DEFECTIMAGEBROWSERDIALOG_H
#define DEFECTIMAGEBROWSERDIALOG_H

#include <QDialog>
#include <QWidget>
#include <QVector>
#include "DefectImageStore.h"

class QLabel;
class QMouseEvent;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

class DefectImageItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DefectImageItemWidget(const DefectImageRecord& record, QWidget* parent = nullptr);

signals:
    void clicked(const DefectImageRecord& record);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    DefectImageRecord mRecord;
};

class DefectImageBrowserDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DefectImageBrowserDialog(DefectImageStore* store, QWidget* parent = nullptr);

    void setPageSize(int pageSize);

private slots:
    void refresh();
    void onRecordsChanged();
    void goPrev();
    void goNext();
    void openPreview(const DefectImageRecord& record);

private:
    void clearRows();
    void updatePager();
    void insertNewItems(int count);

    DefectImageStore* mStore = nullptr;
    int mPageIndex = 0;
    int mPageSize = 20;

    QLabel* mSummaryLabel = nullptr;
    QWidget* mListContainer = nullptr;
    QVBoxLayout* mListLayout = nullptr;
    QScrollArea* mScrollArea = nullptr;
    QPushButton* mPrevButton = nullptr;
    QPushButton* mNextButton = nullptr;
    QLabel* mPageLabel = nullptr;

    QTimer* mDebounceTimer = nullptr;
    int mLastTotalCount = 0;
};

#endif // DEFECTIMAGEBROWSERDIALOG_H
