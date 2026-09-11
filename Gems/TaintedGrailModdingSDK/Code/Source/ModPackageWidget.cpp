/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "ModPackageWidget.h"
#include "FoundationService.h"
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QMetaObject>
namespace TaintedGrailModdingSDK
{
    namespace
    {
        QString PQ(const AZStd::string& s) { return QString::fromUtf8(s.c_str()); }
        AZStd::string PA(const QString& s) { auto b=s.toUtf8(); return {b.constData(),static_cast<size_t>(b.size())}; }
    }
    ModPackageWidget::ModPackageWidget(QWidget* parent):QWidget(parent)
    {
        setObjectName("modPackageBuilder");
        auto* layout=new QVBoxLayout(this);
        auto* title=new QLabel(tr("Mod Package Builder"),this); layout->addWidget(title);
        auto* description=new QLabel(tr("Export editable mod definitions, reviewed images and translations. Reopen a package in a new workspace with your matching local game profile."),this);
        description->setWordWrap(true); layout->addWidget(description);
        auto* row=new QHBoxLayout;
        m_previewButton=new QPushButton(tr("Preview current mod"),this); m_previewButton->setObjectName("packagePreview");
        m_exportButton=new QPushButton(tr("Export package"),this); m_exportButton->setObjectName("packageExport");
        row->addWidget(m_previewButton); row->addWidget(m_exportButton); row->addStretch(); layout->addLayout(row);
        m_inventory=new QTableWidget(this); m_inventory->setObjectName("packageInventory");
        m_inventory->setColumnCount(3); m_inventory->setHorizontalHeaderLabels({tr("Package file"),tr("Bytes"),tr("SHA-256")});
        m_inventory->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_inventory->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
        m_inventory->setColumnWidth(1,90); m_inventory->setColumnWidth(2,200); layout->addWidget(m_inventory,1);
        auto* importGroup=new QGroupBox(tr("Reopen a package"),this);
        auto* form=new QFormLayout(importGroup);
        m_archive=new QLineEdit(importGroup); m_archive->setObjectName("packageArchivePath");
        auto* archiveRow=new QHBoxLayout; auto* browse=new QPushButton(tr("Choose package"),importGroup);
        archiveRow->addWidget(m_archive); archiveRow->addWidget(browse); form->addRow(tr("Archive"),archiveRow);
        m_destination=new QLineEdit(importGroup); m_destination->setObjectName("packageDestination");
        m_destination->setPlaceholderText(tr("Full path to a new workspace folder"));
        auto* destinationRow=new QHBoxLayout; auto* choose=new QPushButton(tr("Choose parent"),importGroup);
        destinationRow->addWidget(m_destination); destinationRow->addWidget(choose); form->addRow(tr("New workspace"),destinationRow);
        auto* actions=new QHBoxLayout;
        m_inspectButton=new QPushButton(tr("Inspect package"),importGroup); m_inspectButton->setObjectName("packageInspect");
        m_importButton=new QPushButton(tr("Create workspace"),importGroup); m_importButton->setObjectName("packageImport");
        m_openButton=new QPushButton(tr("Open created workspace"),importGroup); m_openButton->setObjectName("packageOpen");
        actions->addWidget(m_inspectButton); actions->addWidget(m_importButton); actions->addWidget(m_openButton); form->addRow(actions);
        layout->addWidget(importGroup);
        m_progress=new QProgressBar(this); m_progress->setRange(0,0); m_progress->setVisible(false); layout->addWidget(m_progress);
        m_cancelButton=new QPushButton(tr("Cancel"),this); m_cancelButton->setObjectName("packageCancel"); layout->addWidget(m_cancelButton);
        m_status=new QLabel(tr("Save your mod, then preview its package."),this); m_status->setObjectName("packageStatus"); m_status->setWordWrap(true); m_status->setTextFormat(Qt::PlainText); layout->addWidget(m_status);
        connect(m_previewButton,&QPushButton::clicked,this,[this] { Start(0); });
        connect(m_exportButton,&QPushButton::clicked,this,[this]
        {
            auto* dialog=new QFileDialog(this,tr("Export authoring package"));
            dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setAcceptMode(QFileDialog::AcceptSave);
            dialog->setNameFilter(tr("FOA authoring packages (*.tgmod)")); dialog->setDefaultSuffix("tgmod");
            dialog->setOption(QFileDialog::DontConfirmOverwrite); dialog->setWindowModality(Qt::WindowModal);
            connect(dialog,&QFileDialog::fileSelected,this,[this](QString path)
            { if (!path.endsWith(".tgmod",Qt::CaseInsensitive)) { path+=".tgmod"; } Start(1,path); });
            dialog->open();
        });
        connect(m_inspectButton,&QPushButton::clicked,this,[this] { Start(2); });
        connect(m_importButton,&QPushButton::clicked,this,[this] { Start(3); });
        connect(m_cancelButton,&QPushButton::clicked,this,[this] { if (m_cancel) { m_cancel->store(true); m_status->setText(tr("Cancelling\u2026")); } });
        connect(browse,&QPushButton::clicked,this,[this]
        {
            if (m_busy) { return; }
            auto* dialog=new QFileDialog(this,tr("Choose authoring package"));
            dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setFileMode(QFileDialog::ExistingFile);
            dialog->setNameFilter(tr("FOA authoring packages (*.tgmod)")); dialog->setWindowModality(Qt::WindowModal);
            connect(dialog,&QFileDialog::fileSelected,this,[this](const QString& file) { m_archive->setText(file); Start(2); });
            dialog->open();
        });
        connect(choose,&QPushButton::clicked,this,[this]
        {
            if (m_busy) { return; }
            auto* dialog=new QFileDialog(this,tr("Choose parent for the new workspace"));
            dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setFileMode(QFileDialog::Directory);
            dialog->setOption(QFileDialog::ShowDirsOnly); dialog->setWindowModality(Qt::WindowModal);
            connect(dialog,&QFileDialog::fileSelected,this,[this](const QString& parent)
            { m_destination->setText(QDir(parent).filePath("ImportedMod")); });
            dialog->open();
        });
        connect(m_archive,&QLineEdit::textChanged,this,[this] { m_inspected={}; m_inspectedPath.clear(); UpdateActions(); });
        connect(m_destination,&QLineEdit::textChanged,this,[this] { UpdateActions(); });
        connect(m_openButton,&QPushButton::clicked,this,[this]
        {
            AZStd::string error;
            auto& service=FoundationService::Get();
            // This is a separate explicit context switch, after import has completed.
            if (!service.LoadWorkspace(PA(m_createdWorkspace),&error))
            { m_status->setText(PQ(error)); return; }
            const auto pack=QDir(QFileInfo(m_createdWorkspace).absolutePath()).filePath("Packs/"+m_createdPack+"/pack.tgpack.json");
            if (!service.LoadPack(PA(pack),&error)) { m_status->setText(PQ(error)); return; }
            m_status->setText(tr("Imported workspace opened. Definitions, images and translations are ready to edit."));
        });
        FoundationNotificationBus::Handler::BusConnect(); UpdateActions();
    }
    ModPackageWidget::~ModPackageWidget()
    {
        FoundationNotificationBus::Handler::BusDisconnect();
        if (m_cancel) { m_cancel->store(true); }
        if (m_worker.joinable()) { m_worker.join(); }
    }
    ModPackageContext ModPackageWidget::Context() const
    {
        const auto& f=FoundationService::Get(); ModPackageContext c;
        c.m_workspace=f.GetWorkspace(); c.m_workspaceFile=f.GetWorkspaceFilePath(); c.m_root=f.GetWorkspaceRootPath();
        if (const auto* pack=f.GetActivePack()) { c.m_selectedPack=pack->m_packId; }
        c.m_packs=f.GetPacks(); c.m_catalog=f.GetCatalog(); c.m_evidence=f.GetSourceRegistry(); return c;
    }
    void ModPackageWidget::OnFoundationChanged()
    {
        m_preview={}; m_contextChanged=true;
        if (m_busy && m_cancel) { m_cancel->store(true); }
        UpdateActions();
    }
    void ModPackageWidget::UpdateActions()
    {
        m_previewButton->setEnabled(!m_busy);
        m_exportButton->setEnabled(!m_busy&&!m_preview.m_fingerprint.isEmpty());
        m_inspectButton->setEnabled(!m_busy&&!m_archive->text().isEmpty());
        m_importButton->setEnabled(!m_busy&&!m_inspected.m_fingerprint.isEmpty()&&!m_destination->text().isEmpty());
        m_openButton->setEnabled(!m_busy&&!m_createdWorkspace.isEmpty());
        m_cancelButton->setEnabled(m_busy); m_progress->setVisible(m_busy);
        m_archive->setEnabled(!m_busy); m_destination->setEnabled(!m_busy);
    }
    void ModPackageWidget::ShowPreview(const ModPackagePreview& p)
    {
        m_inventory->setRowCount(p.m_entries.size()); int row=0;
        const auto inventory=QJsonDocument::fromJson(p.m_manifest).object()["entries"].toArray();
        for (auto it=p.m_entries.begin();it!=p.m_entries.end();++it)
        {
            m_inventory->setItem(row,0,new QTableWidgetItem(it.key()));
            m_inventory->setItem(row,1,new QTableWidgetItem(QString::number(it.value().size())));
            const auto hash=inventory[row].toObject()["sha256"].toString();
            m_inventory->setItem(row++,2,new QTableWidgetItem(hash));
        }
        m_status->setText(tr("%1 files \u00b7 %2 bytes \u00b7 %3\n%4").arg(p.m_entries.size()).arg(p.m_totalBytes).arg(p.m_packLabels.join(", "),p.m_warnings.join("\n")));
    }
    void ModPackageWidget::Start(int operation,const QString& output)
    {
        if (m_busy || (operation==1 && output.isEmpty())) { return; }
        if (m_worker.joinable()) { m_worker.join(); }
        const auto context=Context(); const auto preview=m_preview,inspected=m_inspected;
        // A displayed inventory belongs to one operation; never export a different
        // earlier preview while an inspected archive is on screen.
        if (operation==0 || operation==2) { m_preview={}; m_inspected={}; m_inventory->setRowCount(0); }
        const auto archive=m_archive->text(),destination=m_destination->text();
        m_contextChanged=false; m_busy=true; m_cancel=std::make_shared<std::atomic_bool>(false);
        m_status->setText(tr("Working\u2026")); UpdateActions();
        ModPackageProgress progress; progress.m_cancel=m_cancel;
        // Progress is coalesced to avoid a queue entry for every payload.
        progress.m_update=[this](int n,const QString& text)
        { if (n==1 || n%32==0) { QMetaObject::invokeMethod(this,[this,text] { m_status->setText(text); },Qt::QueuedConnection); } };
        m_worker=std::thread([this,operation,context,preview,inspected,archive,destination,output,progress]
        {
            QString error; ModPackagePreview plan; ModPackageReceipt receipt;
            if (operation==0 || operation==2)
            {
                auto r=operation==0 ? ModPackageService::Preview(context,progress) : ModPackageService::Inspect(archive,progress);
                if (r.IsSuccess()) { plan=r.TakeValue(); } else { error=PQ(r.GetError()); }
            }
            else
            {
                auto r=operation==1 ? ModPackageService::Export(context,preview,output,progress)
                    : ModPackageService::Import(archive,inspected.m_fingerprint,context.m_workspace,destination,progress);
                if (r.IsSuccess()) { receipt=r.TakeValue(); } else { error=PQ(r.GetError()); }
            }
            QMetaObject::invokeMethod(this,[this,operation,archive,inspected,plan=AZStd::move(plan),receipt,error]
            {
                if (m_worker.joinable()) { m_worker.join(); }
                m_busy=false;
                if (!error.isEmpty()) { m_status->setText(error); }
                else if (operation==0)
                {
                    if (!m_contextChanged) { m_preview=plan; ShowPreview(plan); }
                    else { m_status->setText(tr("Workspace changed. Preview again.")); }
                }
                else if (operation==2) { m_inspected=plan; m_inspectedPath=archive; ShowPreview(plan); }
                else if (operation==1) { m_status->setText(tr("Package exported and verified: %1").arg(receipt.m_path)); m_archive->setText(receipt.m_path); }
                else
                {
                    m_createdWorkspace=receipt.m_path; m_createdPack=inspected.m_selectedPack;
                    m_status->setText(tr("New workspace created and verified. Open it when you are ready: %1").arg(receipt.m_path));
                }
                UpdateActions();
            },Qt::QueuedConnection);
        });
    }
}
