/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of Natron <http://www.natron.fr/>,
 * Copyright (C) 2016 INRIA and Alexandre Gauthier-Foichat
 *
 * Natron is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Natron is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Natron.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
 * ***** END LICENSE BLOCK ***** */

// ***** BEGIN PYTHON BLOCK *****
// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>
// ***** END PYTHON BLOCK *****

#include "NodeViewerContext.h"

#include <QMouseEvent>
#include <QStyle>
#include <QToolBar>
#include <QAction>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPalette>
#include <QFrame>
#include <QUndoStack>
#include <QUndoCommand>
#include <ofxKeySyms.h>

#include "Engine/KnobTypes.h"
#include "Engine/Node.h"
#include "Engine/EffectInstance.h"
#include "Engine/Plugin.h"

#include "Gui/ActionShortcuts.h"
#include "Gui/ClickableLabel.h"
#include "Gui/GuiApplicationManager.h"
#include "Gui/GuiDefines.h"
#include "Gui/GuiMacros.h"
#include "Gui/KnobGui.h"
#include "Gui/NodeGui.h"
#include "Gui/NodeSettingsPanel.h"
#include "Gui/QtEnumConvert.h"
#include "Gui/Utils.h"
#include "Gui/ViewerGL.h"
#include "Gui/ViewerTab.h"
#include "Gui/ViewerToolButton.h"

NATRON_NAMESPACE_ENTER;

struct NodeViewerContextPrivate
{
    Q_DECLARE_TR_FUNCTIONS(NodeViewerContext)

public:
    NodeViewerContext* publicInterface;
    NodeGuiWPtr node;
    ViewerGL* viewer;
    ViewerTab* viewerTab;
    std::map<KnobWPtr, KnobGuiPtr> knobsMapping;
    QString currentRole, currentTool;
    QToolBar* toolbar;
    std::map<QString, ViewerToolButton*> toolButtons;
    QWidget* mainContainer;
    QVBoxLayout* mainContainerLayout;

    NodeViewerContextPrivate(NodeViewerContext* pi,
                             const NodeGuiPtr& node,
                             ViewerTab* viewer)
        : publicInterface(pi)
        , node(node)
        , viewer( viewer->getViewer() )
        , viewerTab(viewer)
        , knobsMapping()
        , currentRole()
        , currentTool()
        , toolbar(0)
        , toolButtons()
        , mainContainer(0)
        , mainContainerLayout(0)
    {
    }

    void createKnobs(const KnobsVec& knobsUi);

    NodeGuiPtr getNode() const
    {
        return node.lock();
    }

    /**
     * @brief Add a new tool to the toolbutton corresponding to the given roleID.
     * If a ViewerToolButton for this roleID does not exist yet, it is created and the
     * given roleShortcutID is associated to it.
     * The shortcut for this button will be added to the Shorcut Editor with by default
     * the given modifiers and symbols (as defined in <ofxKeySyms.h>).
     * The tool will have the given label, and when hovering the button with the mouse, the user
     * will receive the hintToolTip help.
     * Optionnally, a path to an icon can be specified for this button.
     **/
    void addToolBarTool(const std::string& toolID,
                        const std::string& roleID,
                        const std::string& roleShortcutID,
                        const std::string& label,
                        const std::string& hintToolTip,
                        const std::string& iconPath);

    void onToolActionTriggeredInternal(QAction* action, bool notifyNode);

    void toggleToolsSelection(ViewerToolButton* selected)
    {
        for (std::map<QString, ViewerToolButton*>::iterator it = toolButtons.begin(); it != toolButtons.end(); ++it) {
            if (it->second == selected) {
                it->second->setIsSelected(true);
            } else {
                it->second->setIsSelected(false);
            }
        }
    }
};

NodeViewerContext::NodeViewerContext(const NodeGuiPtr& node,
                                     ViewerTab* viewer)
    : QObject()
    , KnobGuiContainerI()
    , _imp( new NodeViewerContextPrivate(this, node, viewer) )
{


}

NodeViewerContext::~NodeViewerContext()
{
}

void
NodeViewerContext::createGui()
{

    QObject::connect( _imp->viewer, SIGNAL(selectionRectangleChanged(bool)), this, SLOT(updateSelectionFromViewerSelectionRectangle(bool)), Qt::UniqueConnection );
    QObject::connect( _imp->viewer, SIGNAL(selectionCleared()), this, SLOT(onViewerSelectionCleared()), Qt::UniqueConnection );

    NodeGuiPtr node = _imp->getNode();
    QObject::connect( node.get(), SIGNAL(settingsPanelClosed(bool)), this, SLOT(onNodeSettingsPanelClosed(bool)), Qt::UniqueConnection );


    KnobsVec knobsOrdered = node->getNode()->getEffectInstance()->getViewerUIKnobs();


    if (!knobsOrdered.empty()) {
        _imp->mainContainer = new QWidget(_imp->viewer);
        _imp->mainContainerLayout = new QVBoxLayout(_imp->mainContainer);
        _imp->mainContainerLayout->setContentsMargins(0, 0, 0, 0);
        _imp->mainContainerLayout->setSpacing(0);
        setContainerWidget(_imp->mainContainer);
        _imp->createKnobs(knobsOrdered);

    }

    const KnobsVec& allKnobs = node->getNode()->getKnobs();
    KnobPage* toolbarPage = 0;
    for (KnobsVec::const_iterator it = allKnobs.begin(); it != allKnobs.end(); ++it) {
        KnobPage* isPage = dynamic_cast<KnobPage*>(it->get());
        if (isPage && isPage->getIsToolBar()) {
            toolbarPage = isPage;
            break;
        }
    }
    if (toolbarPage) {
        std::vector<KnobPtr> pageChildren = toolbarPage->getChildren();
        if (!pageChildren.empty()) {
            _imp->toolbar = new QToolBar(_imp->viewer);
            _imp->toolbar->setOrientation(Qt::Vertical);

            for (std::size_t i = 0; i < pageChildren.size(); ++i) {
                KnobGroup* isGroup = dynamic_cast<KnobGroup*>(pageChildren[i].get());
                if (isGroup) {
                    std::vector<KnobPtr> toolButtonChildren = isGroup->getChildren();
                    for (std::size_t j = 0; j < toolButtonChildren.size(); ++j) {
                        KnobButton* isButton = dynamic_cast<KnobButton*>(toolButtonChildren[j].get());
                        if (isButton) {
                            const std::string& roleShortcutID = isGroup->getName();
                            _imp->addToolBarTool(isButton->getName(), isGroup->getName(), roleShortcutID, isButton->getLabel(), isButton->getHintToolTip(), isButton->getIconLabel());
                        }
                    }
                }
            }
        }
    }
}

static void
addSpacer(QBoxLayout* layout)
{
    layout->addSpacing( TO_DPIX(5) );
    QFrame* line = new QFrame( layout->parentWidget() );
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Raised);
    QPalette palette;
    palette.setColor(QPalette::Foreground, Qt::black);
    line->setPalette(palette);
    layout->addWidget(line);
    layout->addSpacing( TO_DPIX(5) );
}

void
NodeViewerContext::onNodeSettingsPanelClosed(bool closed)
{
    if (!_imp->viewerTab) {
        return;

    }
    NodeGuiPtr node = _imp->node.lock();
    if (closed) {
        _imp->viewerTab->removeNodeViewerInterface(node, false /*permanantly*/, true /*setAnother*/);
    } else {
        // Set the viewer interface for this plug-in to be the one of this node
        _imp->viewerTab->setPluginViewerInterface(node);
    }

}

int
NodeViewerContext::getItemsSpacingOnSameLine() const
{
    return 0;
}

void
NodeViewerContextPrivate::createKnobs(const KnobsVec& knobsOrdered)
{
    NodeGuiPtr thisNode = getNode();
    assert(!knobsOrdered.empty());


    knobsMapping.clear();


    QWidget* lastRowContainer = new QWidget(mainContainer);
    QHBoxLayout* lastRowLayout = new QHBoxLayout(lastRowContainer);
    lastRowLayout->setContentsMargins(TO_DPIX(3), TO_DPIY(2), 0, 0);
    lastRowLayout->setSpacing(0);
    mainContainerLayout->addWidget(lastRowContainer);

    KnobsVec knobsOnSameLine;

    KnobsVec::const_iterator next = knobsOrdered.begin();

    ++next;
    for (KnobsVec::const_iterator it = knobsOrdered.begin(); it != knobsOrdered.end(); ++it) {
        KnobGuiPtr ret( appPTR->createGuiForKnob(*it, publicInterface) );
        if (!ret) {
            assert(false);
            continue;
        }
        ret->initialize();

        knobsMapping.insert(std::make_pair(*it, ret));

        bool makeNewLine = (*it)->getInViewerContextNewLineActivated();

        KnobClickableLabel* label = 0;
        std::string inViewerLabel = (*it)->getInViewerContextLabel();
        if (!inViewerLabel.empty()) {
            label = new KnobClickableLabel(QString::fromUtf8(inViewerLabel.c_str()), ret, mainContainer);
        }
        ret->createGUI(lastRowContainer, 0, label, 0 /*warningIndicator*/, lastRowLayout, makeNewLine, knobsOnSameLine);

        if (makeNewLine) {
            knobsOnSameLine.clear();
            lastRowLayout->addStretch();
            lastRowContainer = new QWidget(mainContainer);
            lastRowLayout = new QHBoxLayout(lastRowContainer);
            lastRowLayout->setContentsMargins(TO_DPIX(3), TO_DPIY(2), 0, 0);
            lastRowLayout->setSpacing(0);
            mainContainerLayout->addWidget(lastRowContainer);
        } else {
            knobsOnSameLine.push_back(*it);
            if (next != knobsOrdered.end()) {
                if ((*it)->getInViewerContextAddSeparator()) {
                    addSpacer(lastRowLayout);
                } else {
                    int spacing = (*it)->getInViewerContextItemSpacing();
                    lastRowLayout->addSpacing(TO_DPIX(spacing));

                }
            }
        } // makeNewLine

        if ( next == knobsOrdered.end() ) {
            ++next;
        }
    }
    lastRowLayout->addStretch();
} // NodeViewerContextPrivate::createKnobs

void
NodeViewerContextPrivate::addToolBarTool(const std::string& toolID,
                                         const std::string& roleID,
                                         const std::string& roleShortcutID,
                                         const std::string& label,
                                         const std::string& hintToolTip,
                                         const std::string& iconPath)
{
    QString qRoleId = QString::fromUtf8( roleID.c_str() );
    std::map<QString, ViewerToolButton*>::iterator foundToolButton = toolButtons.find(qRoleId);
    ViewerToolButton* toolButton = 0;

    if ( foundToolButton != toolButtons.end() ) {
        toolButton = foundToolButton->second;
    } else {
        toolButton = new ViewerToolButton(toolbar);
        toolButtons.insert( std::make_pair(qRoleId, toolButton) );
        QSize rotoToolSize( TO_DPIX(NATRON_LARGE_BUTTON_SIZE), TO_DPIY(NATRON_LARGE_BUTTON_SIZE) );
        toolButton->setFixedSize(rotoToolSize);
        toolButton->setIconSize(rotoToolSize);
        toolButton->setPopupMode(QToolButton::InstantPopup);
        QObject::connect( toolButton, SIGNAL(triggered(QAction*)), publicInterface, SLOT(onToolActionTriggered(QAction*)) );
    }


    QString shortcutGroup = getNode()->getNode()->getPlugin()->getPluginShortcutGroup();
    QIcon icon;
    if ( !iconPath.empty() ) {
        QString iconPathStr = QString::fromUtf8( iconPath.c_str() );
        if ( QFile::exists(iconPathStr) ) {
            QPixmap pix;
            pix.load(iconPathStr);
            if ( !pix.isNull() ) {
                icon.addPixmap(pix);
            }
        }
    }

    QString labelTouse = icon.isNull() ? QString::fromUtf8(label.c_str()) : QString();
    QAction* action = new QAction(icon, labelTouse, toolButton);



    QStringList data;
    data.push_back(qRoleId);
    data.push_back( QString::fromUtf8( toolID.c_str() ) );
    action->setData(data);
    QString toolTip;
    if ( !hintToolTip.empty() ) {
        toolTip.append( QString::fromUtf8("<p>") );
        toolTip += QString::fromUtf8( hintToolTip.c_str() );
        toolTip.append( QString::fromUtf8("</p>") );
        if ( !roleShortcutID.empty() ) {
            std::list<QKeySequence> keybinds = getKeybind( shortcutGroup, QString::fromUtf8( toolID.c_str() ) );
            if (keybinds.size() >= 1) {
                toolTip += QString::fromUtf8("<p><b>");
                toolTip += tr("Keyboard shortcut: %1").arg( keybinds.front().toString(QKeySequence::NativeText) );
                toolTip += QString::fromUtf8("</b></p>");
            }
        }
        action->setToolTip(toolTip);
    }
    QObject::connect( action, SIGNAL(triggered()), publicInterface, SLOT(onToolActionTriggered()) );
    toolButton->addAction(action);
} // NodeViewerContextPrivate::addToolBarTool

QToolBar*
NodeViewerContext::getToolBar() const
{
    return _imp->toolbar;
}

const QString&
NodeViewerContext::getCurrentRole() const
{
    return _imp->currentRole;
}

const QString&
NodeViewerContext::getCurrentTool() const
{
    return _imp->currentTool;
}

Gui*
NodeViewerContext::getGui() const
{
    return _imp->viewerTab ? _imp->viewerTab->getGui() : 0;
}

const QUndoCommand*
NodeViewerContext::getLastUndoCommand() const
{
    NodeSettingsPanel* panel = _imp->getNode()->getSettingPanel();

    if (panel) {
        return panel->getLastUndoCommand();
    }

    return 0;
}

void
NodeViewerContext::pushUndoCommand(QUndoCommand* cmd)
{
    NodeSettingsPanel* panel = _imp->getNode()->getSettingPanel();

    if (panel) {
        panel->pushUndoCommand(cmd);
    }
}

KnobGuiPtr
NodeViewerContext::getKnobGui(const KnobPtr& knob) const
{
    std::map<KnobWPtr, KnobGuiPtr>::const_iterator found =  _imp->knobsMapping.find(knob);

    if ( found == _imp->knobsMapping.end() ) {
        return KnobGuiPtr();
    }

    return found->second;
}

void
NodeViewerContext::onToolActionTriggered()
{
    QAction* act = qobject_cast<QAction*>( sender() );

    if (act) {
        onToolActionTriggered(act);
    }
}

void
NodeViewerContext::onToolActionTriggered(QAction* act)
{
    _imp->onToolActionTriggeredInternal(act, true);
}

void
NodeViewerContext::setCurrentTool(const QString& toolID,
                                  bool notifyNode)
{
    QList<QAction*> actions;
    for (std::map<QString, ViewerToolButton*>::iterator it = _imp->toolButtons.begin(); it != _imp->toolButtons.end(); ++it) {
        QList<QAction*> roleActions = it->second->actions();
        actions.append(roleActions);
    }
    for (int i = 0; i < actions.size(); ++i) {
        QStringList actionData = actions[i]->data().toStringList();
        if (actionData.size() != 2) {
            continue;
        }
        if (actionData[1] == toolID) {
            _imp->onToolActionTriggeredInternal(actions[i], notifyNode);

            return;
        }
    }
}

void
NodeViewerContext::onToolGroupValueChanged(ViewSpec /*view*/,
                             int /*dimension*/,
                             int reason)
{
    KnobSignalSlotHandler* caller = dynamic_cast<KnobSignalSlotHandler*>(sender());
    if (!caller) {
        return;
    }
    KnobPtr knob = caller->getKnob();
    if (!knob) {
        return;
    }

    if (reason == eValueChangedReasonNatronGuiEdited ||
        reason == eValueChangedReasonUserEdited) {
        return;
    }

    QString newRoleID = QString::fromUtf8(knob->getName().c_str());

    std::map<QString,ViewerToolButton*>::iterator foundOldTool = _imp->toolButtons.find(newRoleID);
    assert(foundOldTool != _imp->toolButtons.end());
    if (foundOldTool == _imp->toolButtons.end()) {
        return;
    }

    ViewerToolButton* newToolButton = foundOldTool->second;
    assert(newToolButton);
    _imp->toggleToolsSelection(newToolButton);
    newToolButton->setDown(true);

    _imp->currentRole = newRoleID;

}

void
NodeViewerContext::onToolActionValueChanged(ViewSpec /*view*/,
                              int /*dimension*/,
                              int reason)
{
    KnobSignalSlotHandler* caller = dynamic_cast<KnobSignalSlotHandler*>(sender());
    if (!caller) {
        return;
    }
    KnobPtr knob = caller->getKnob();
    if (!knob) {
        return;
    }

    if (reason == eValueChangedReasonNatronGuiEdited ||
        reason == eValueChangedReasonUserEdited) {
        return;
    }

    QString newToolID = QString::fromUtf8(knob->getName().c_str());

    std::map<QString,ViewerToolButton*>::iterator foundOldTool = _imp->toolButtons.find(_imp->currentRole);
    assert(foundOldTool != _imp->toolButtons.end());
    if (foundOldTool == _imp->toolButtons.end()) {
        return;
    }

    ViewerToolButton* newToolButton = foundOldTool->second;
    assert(newToolButton);
    QList<QAction*> actions = newToolButton->actions();
    for (QList<QAction*>::iterator it = actions.begin(); it != actions.end(); ++it) {
        QStringList actionData = (*it)->data().toStringList();

        if (actionData.size() != 2) {
            continue;
        }
        const QString& actionRoleID = actionData[0];
        const QString& actionTool = actionData[1];
        assert(actionRoleID == _imp->currentRole);
        if (actionRoleID == _imp->currentRole && actionTool == newToolID) {
            newToolButton->setDefaultAction(*it);
            _imp->currentTool = newToolID;
            return;
        }

    }




}

void
NodeViewerContextPrivate::onToolActionTriggeredInternal(QAction* action, bool notifyNode)
{
    QStringList actionData = action->data().toStringList();

    if (actionData.size() != 2) {
        return;
    }


    const QString& newRoleID = actionData[0];
    const QString& newToolID = actionData[1];

    if (currentTool == newToolID) {
        return;
    }

    std::map<QString, ViewerToolButton*>::iterator foundOldTool = toolButtons.find(newRoleID);
    assert( foundOldTool != toolButtons.end() );
    if ( foundOldTool == toolButtons.end() ) {
        return;
    }

    ViewerToolButton* newToolButton = foundOldTool->second;
    assert(newToolButton);
    toggleToolsSelection(newToolButton);
    newToolButton->setDown(true);
    newToolButton->setDefaultAction(action);

    QString oldRole = currentRole;
    QString oldTool = currentTool;

    currentRole = newRoleID;
    currentTool = newToolID;

    if (notifyNode) {

        // Refresh other viewers toolbars
        NodeGuiPtr n = node.lock();
        const std::list<ViewerTab*> viewers = publicInterface->getGui()->getViewersList();
        for (std::list<ViewerTab*>::const_iterator it = viewers.begin(); it != viewers.end(); ++it) {
            if (*it != viewerTab) {
                (*it)->updateSelectedToolForNode(newToolID, n);
            }
        }

        KnobPtr oldGroupKnob = n->getNode()->getKnobByName(oldRole.toStdString());
        KnobPtr newGroupKnob = n->getNode()->getKnobByName(newRoleID.toStdString());

        KnobPtr oldToolKnob = n->getNode()->getKnobByName(oldTool.toStdString());
        KnobPtr newToolKnob = n->getNode()->getKnobByName(newToolID.toStdString());
        assert(oldToolKnob && newToolKnob && oldGroupKnob && newGroupKnob);
        if (oldToolKnob && newToolKnob && oldGroupKnob && newGroupKnob) {
            KnobButton* oldIsButton = dynamic_cast<KnobButton*>(oldToolKnob.get());
            assert(oldIsButton);
            KnobButton* newIsButton = dynamic_cast<KnobButton*>(newToolKnob.get());
            assert(newIsButton);

            KnobGroup* oldIsGroup = dynamic_cast<KnobGroup*>(oldGroupKnob.get());
            assert(oldIsGroup);
            KnobGroup* newIsGroup = dynamic_cast<KnobGroup*>(newGroupKnob.get());
            assert(newIsGroup);
            if (oldIsButton && newIsButton && oldIsGroup && newIsGroup) {

                oldIsGroup->onValueChanged(false, ViewSpec::all(), 0, eValueChangedReasonUserEdited, 0);
                newIsGroup->onValueChanged(true, ViewSpec::all(), 0, eValueChangedReasonUserEdited, 0);


                oldIsButton->onValueChanged(false, ViewSpec::all(), 0, eValueChangedReasonUserEdited, 0);
                newIsButton->onValueChanged(true, ViewSpec::all(), 0, eValueChangedReasonUserEdited, 0);
            }
        }

    }

}

void
NodeViewerContext::updateSelectionFromViewerSelectionRectangle(bool onRelease)
{
    NodeGuiPtr n = _imp->getNode();
    if (n) {
        return;
    }
    NodePtr node = n->getNode();
    if (!node) {
        return;
    }
    RectD rect;
    _imp->viewer->getSelectionRectangle(rect.x1, rect.x2, rect.y1, rect.y2);
    node->getEffectInstance()->onInteractViewportSelectionUpdated(rect, onRelease);
}

void
NodeViewerContext::onViewerSelectionCleared()
{
    NodeGuiPtr n = _imp->getNode();
    if (n) {
        return;
    }
    NodePtr node = n->getNode();
    if (!node) {
        return;
    }
    node->getEffectInstance()->onInteractViewportSelectionCleared();
}

void
NodeViewerContext::notifyGuiClosing()
{
    _imp->viewer = 0;
    _imp->viewerTab = 0;
}

NATRON_NAMESPACE_EXIT;
NATRON_NAMESPACE_USING;
#include "moc_NodeViewerContext.cpp"