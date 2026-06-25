#include "DefectImageBrowserDialog.h"
#include "Theme.h"

#include <QApplication>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>

namespace {

QString buttonStyle(const QString& bg, const QString& hover)
{
    return QString(
        "QPushButton { background-color: %1; color: #e0f0ff; border: 1px solid #1a3a60; "
        "padding: 6px 12px; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:disabled { background-color: #101a30; color: #506080; border-color: #182840; }")
        .arg(bg, hover);
}

class ImagePreviewDialog : public QDialog
{
public:
    explicit ImagePreviewDialog(const DefectImageRecord& record, QWidget* parent = nullptr)
        : QDialog(parent)
        , mRecord(record)
    {
        setWindowTitle(QString::fromUtf8("异常图片 - %1").arg(record.location));
        resize(1100, 760);
        setMinimumSize(720, 480);
        buildUi();
        loadImage();
    }

protected:
    void showEvent(QShowEvent* event) override
    {
        QDialog::showEvent(event);
        fitToWindow();
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QDialog::resizeEvent(event);
        if (mFitMode)
            fitToWindow();
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (event->modifiers() & Qt::ControlModifier) {
            if (event->angleDelta().y() > 0)
                zoomBy(1.15);
            else
                zoomBy(1.0 / 1.15);
            event->accept();
            return;
        }
        QDialog::wheelEvent(event);
    }

private:
    void buildUi()
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(12, 12, 12, 12);
        root->setSpacing(10);

        auto* top = new QHBoxLayout();
        auto* title = new QLabel(
            QString::fromUtf8("%1  |  %2  |  异常 %3%")
                .arg(mRecord.location)
                .arg(mRecord.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")))
                .arg(qRound(mRecord.confidence * 100.0f)), this);
        title->setStyleSheet("color: #e0f0ff; font-size: 14px; font-weight: bold;");
        top->addWidget(title, 1);

        auto* zoomOut = new QPushButton("-", this);
        auto* reset = new QPushButton(QString::fromUtf8("适配"), this);
        auto* zoomIn = new QPushButton("+", this);
        auto* closeBtn = new QPushButton("X", this);
        zoomOut->setFixedWidth(40);
        zoomIn->setFixedWidth(40);
        closeBtn->setFixedWidth(42);
        const QString btnStyle = buttonStyle("#142a50", "#1d3d70");
        zoomOut->setStyleSheet(btnStyle);
        zoomIn->setStyleSheet(btnStyle);
        reset->setStyleSheet(btnStyle);
        closeBtn->setStyleSheet(buttonStyle("#602020", "#803030"));
        top->addWidget(zoomOut);
        top->addWidget(reset);
        top->addWidget(zoomIn);
        top->addWidget(closeBtn);
        root->addLayout(top);

        mImageLabel = new QLabel(this);
        mImageLabel->setAlignment(Qt::AlignCenter);
        mImageLabel->setStyleSheet(
            QString("QLabel { background: %1; color: %2; }")
                .arg(Theme::background().darker(150).name())
                .arg(Theme::textMuted().name()));

        mScrollArea = new QScrollArea(this);
        mScrollArea->setWidget(mImageLabel);
        mScrollArea->setWidgetResizable(false);
        mScrollArea->setFrameShape(QFrame::NoFrame);
        mScrollArea->setStyleSheet(
            QString("QScrollArea { background: %1; border: 1px solid %2; }")
                .arg(Theme::background().darker(150).name())
                .arg(Theme::accentDim().name()));
        root->addWidget(mScrollArea, 1);

        connect(zoomOut, &QPushButton::clicked, this, [this]() { zoomBy(1.0 / 1.25); });
        connect(zoomIn, &QPushButton::clicked, this, [this]() { zoomBy(1.25); });
        connect(reset, &QPushButton::clicked, this, [this]() { fitToWindow(); });
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

        setStyleSheet(
            QString("QDialog { background-color: %1; color: %2; }"
                    "QScrollBar:vertical, QScrollBar:horizontal { background: %1; width: 10px; height: 10px; }"
                    "QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background: %3; border-radius: 4px; }")
                .arg(Theme::background().name())
                .arg(Theme::textPrimary().name())
                .arg(Theme::accentDim().name()));
    }

    void loadImage()
    {
        if (!QFileInfo::exists(mRecord.filePath) || !mOriginal.load(mRecord.filePath)) {
            mImageLabel->setText(QString::fromUtf8("图片不存在或读取失败"));
            mImageLabel->setMinimumSize(600, 360);
            return;
        }
        fitToWindow();
    }

    void fitToWindow()
    {
        if (mOriginal.isNull())
            return;
        QSize viewport = mScrollArea->viewport()->size();
        if (viewport.width() <= 0 || viewport.height() <= 0)
            return;
        double sx = static_cast<double>(viewport.width() - 16) / mOriginal.width();
        double sy = static_cast<double>(viewport.height() - 16) / mOriginal.height();
        mScale = qBound(0.1, std::min(sx, sy), 8.0);
        mFitMode = true;
        updatePixmap();
    }

    void zoomBy(double factor)
    {
        if (mOriginal.isNull())
            return;
        mScale = qBound(0.1, mScale * factor, 8.0);
        mFitMode = false;
        updatePixmap();
    }

    void updatePixmap()
    {
        QSize scaledSize(
            std::max(1, qRound(mOriginal.width() * mScale)),
            std::max(1, qRound(mOriginal.height() * mScale)));
        QPixmap scaled = mOriginal.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        mImageLabel->setPixmap(scaled);
        mImageLabel->resize(scaled.size());
    }

    DefectImageRecord mRecord;
    QPixmap mOriginal;
    QLabel* mImageLabel = nullptr;
    QScrollArea* mScrollArea = nullptr;
    double mScale = 1.0;
    bool mFitMode = true;
};

} // namespace

DefectImageItemWidget::DefectImageItemWidget(const DefectImageRecord& record, QWidget* parent)
    : QWidget(parent)
    , mRecord(record)
{
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(92);
    setStyleSheet(
        "QWidget { background: rgba(16,26,50,200); border-radius: 4px; }"
        "QLabel { color: #e0f0ff; background: transparent; }");

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(8, 8, 8, 8);
    row->setSpacing(10);

    auto* thumb = new QLabel(this);
    thumb->setFixedSize(120, 68);
    thumb->setAlignment(Qt::AlignCenter);
    thumb->setStyleSheet("QLabel { background: #050812; border: 1px solid #1a3a60; color: #607090; }");

    QImageReader reader(record.filePath);
    QSize sourceSize = reader.size();
    if (sourceSize.isValid())
        reader.setScaledSize(sourceSize.scaled(thumb->size(), Qt::KeepAspectRatio));
    QImage image = reader.read();
    if (!image.isNull()) {
        thumb->setPixmap(QPixmap::fromImage(image));
    } else {
        thumb->setText(QString::fromUtf8("无图片"));
    }
    row->addWidget(thumb);

    auto* info = new QVBoxLayout();
    info->setSpacing(4);

    auto* loc = new QLabel(record.location, this);
    loc->setStyleSheet("QLabel { color: #e0f0ff; font-size: 13px; font-weight: bold; }");
    auto* detail = new QLabel(
        QString::fromUtf8("异常 | %1%").arg(qRound(record.confidence * 100.0f)), this);
    detail->setStyleSheet("QLabel { color: #ff5252; font-size: 12px; }");
    auto* time = new QLabel(record.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")), this);
    time->setStyleSheet("QLabel { color: #7f9bc6; font-size: 11px; }");

    info->addWidget(loc);
    info->addWidget(detail);
    info->addWidget(time);
    info->addStretch();
    row->addLayout(info, 1);
}

void DefectImageItemWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked(mRecord);
    QWidget::mousePressEvent(event);
}

DefectImageBrowserDialog::DefectImageBrowserDialog(DefectImageStore* store, QWidget* parent)
    : QDialog(parent)
    , mStore(store)
{
    setWindowTitle(QString::fromUtf8("异常图片"));
    setMinimumSize(820, 620);
    resize(900, 680);

    if (mStore)
        mPageSize = mStore->pageSize();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    auto* header = new QHBoxLayout();
    auto* title = new QLabel(QString::fromUtf8("异常图片"), this);
    title->setStyleSheet("QLabel { color: #e0f0ff; font-size: 18px; font-weight: bold; }");
    mSummaryLabel = new QLabel(this);
    mSummaryLabel->setStyleSheet("QLabel { color: #7f9bc6; font-size: 12px; }");
    header->addWidget(title);
    header->addStretch();
    header->addWidget(mSummaryLabel);
    root->addLayout(header);

    mListContainer = new QWidget(this);
    mListLayout = new QVBoxLayout(mListContainer);
    mListLayout->setContentsMargins(0, 0, 0, 0);
    mListLayout->setSpacing(8);
    mListLayout->addStretch();

    mListContainer->setStyleSheet("QWidget { background: transparent; }");

    mScrollArea = new QScrollArea(this);
    mScrollArea->setWidgetResizable(true);
    mScrollArea->setFrameShape(QFrame::NoFrame);
    mScrollArea->setWidget(mListContainer);
    mScrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    root->addWidget(mScrollArea, 1);

    auto* pager = new QHBoxLayout();
    mPrevButton = new QPushButton(QString::fromUtf8("上一页"), this);
    mNextButton = new QPushButton(QString::fromUtf8("下一页"), this);
    mPageLabel = new QLabel(this);
    mPageLabel->setAlignment(Qt::AlignCenter);
    mPageLabel->setStyleSheet("QLabel { color: #a0c8ff; font-size: 12px; }");
    const QString btnStyle = buttonStyle("#142a50", "#1d3d70");
    mPrevButton->setStyleSheet(btnStyle);
    mNextButton->setStyleSheet(btnStyle);
    pager->addWidget(mPrevButton);
    pager->addStretch();
    pager->addWidget(mPageLabel);
    pager->addStretch();
    pager->addWidget(mNextButton);
    root->addLayout(pager);

    connect(mPrevButton, &QPushButton::clicked, this, &DefectImageBrowserDialog::goPrev);
    connect(mNextButton, &QPushButton::clicked, this, &DefectImageBrowserDialog::goNext);
    if (mStore)
        connect(mStore, &DefectImageStore::recordsChanged, this, &DefectImageBrowserDialog::refresh);

    setStyleSheet(
        QString("QDialog { background-color: %1; color: %2; }"
                "QScrollBar:vertical { background: %1; width: 8px; }"
                "QScrollBar::handle:vertical { background: %3; border-radius: 4px; min-height: 24px; }")
            .arg(Theme::background().name())
            .arg(Theme::textPrimary().name())
            .arg(Theme::accentDim().name()));

    refresh();
}

void DefectImageBrowserDialog::setPageSize(int pageSize)
{
    mPageSize = std::max(1, pageSize);
    if (mStore)
        mStore->setPageSize(mPageSize);
    mPageIndex = 0;
    refresh();
}

void DefectImageBrowserDialog::refresh()
{
    clearRows();

    if (!mStore) {
        mSummaryLabel->setText(QString::fromUtf8("无数据源"));
        updatePager();
        return;
    }

    int totalPages = mStore->pageCount(mPageSize);
    if (mPageIndex >= totalPages)
        mPageIndex = totalPages - 1;
    if (mPageIndex < 0)
        mPageIndex = 0;

    QVector<DefectImageRecord> records = mStore->recordsForPage(mPageIndex, mPageSize);
    if (records.isEmpty()) {
        auto* empty = new QLabel(QString::fromUtf8("暂无异常图片"), mListContainer);
        empty->setAlignment(Qt::AlignCenter);
        empty->setMinimumHeight(180);
        empty->setStyleSheet("QLabel { color: #7080a0; font-size: 14px; }");
        mListLayout->insertWidget(0, empty);
    } else {
        for (const auto& record : records) {
            auto* item = new DefectImageItemWidget(record, mListContainer);
            connect(item, &DefectImageItemWidget::clicked,
                    this, &DefectImageBrowserDialog::openPreview);
            mListLayout->insertWidget(mListLayout->count() - 1, item);
        }
    }

    mScrollArea->verticalScrollBar()->setValue(0);
    updatePager();
}

void DefectImageBrowserDialog::goPrev()
{
    if (mPageIndex > 0) {
        --mPageIndex;
        refresh();
    }
}

void DefectImageBrowserDialog::goNext()
{
    if (!mStore)
        return;
    if (mPageIndex + 1 < mStore->pageCount(mPageSize)) {
        ++mPageIndex;
        refresh();
    }
}

void DefectImageBrowserDialog::openPreview(const DefectImageRecord& record)
{
    if (!QFileInfo::exists(record.filePath)) {
        QMessageBox::warning(this, QString::fromUtf8("图片不可用"),
                             QString::fromUtf8("图片文件不存在:\n%1").arg(record.filePath));
        return;
    }

    auto* dlg = new ImagePreviewDialog(record, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void DefectImageBrowserDialog::clearRows()
{
    if (!mListLayout)
        return;

    while (mListLayout->count() > 1) {
        QLayoutItem* item = mListLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void DefectImageBrowserDialog::updatePager()
{
    int total = mStore ? mStore->count() : 0;
    int pages = mStore ? mStore->pageCount(mPageSize) : 1;
    mSummaryLabel->setText(QString::fromUtf8("共 %1 张").arg(total));
    mPageLabel->setText(QString::fromUtf8("第 %1 / %2 页  |  每页 %3 张")
                            .arg(mPageIndex + 1)
                            .arg(pages)
                            .arg(mPageSize));
    mPrevButton->setEnabled(mPageIndex > 0);
    mNextButton->setEnabled(mStore && mPageIndex + 1 < pages);
}
