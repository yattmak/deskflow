/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Log.h"

#include <QObject>

class ServerProxyTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void handleKeepAliveAlarm_timeout_queuesDisconnectRequest();
  void handleData_incompleteMessage_queuesDisconnectRequest();
  void parseHandshakeMessage_protocolError_queuesRefusalRequest();
  void modifierTranslation_generic_data();
  void modifierTranslation_generic();
  void modifierTranslation_sideInheritance_data();
  void modifierTranslation_sideInheritance();
  void modifierTranslation_hangulDownRepeatUp_consumesMask();
  void modifierTranslation_altGrMaskOmission_isRespected();
  void modifierTranslation_bothHeldRelease_data();
  void modifierTranslation_bothHeldRelease();
  void modifierTranslation_bothOverrides_composesMasks();
  void modifierTranslation_sharedTarget_preservesOtherSource();
  void modifierTranslation_sharedPhysicalTarget_data();
  void modifierTranslation_sharedPhysicalTarget();
  void modifierTranslation_sharedOwnerButton_reuseDoesNotAlias();
  void modifierTranslation_sharedPhysicalTarget_deactivateReleasesOnce();
  void modifierTranslation_invalidTarget_keepsMapping();
  void modifierTranslation_namedKeysAndAliases();
  void modifierTranslation_optionsChangedWhileHeld_retainsPressTarget();
  void modifierTranslation_deactivate_releasesKeysAndClearsSides();
  void modifierTranslation_reset_restoresInheritance();
  void modifierTranslation_resetWhileHeld_preservesPressTargets();
  void modifierTranslation_resetSharedTarget_preservesOwner();
  void modifierTranslation_reenterAmbiguousMask_avoidsShortcut();
  void modifierTranslation_lockTargets_areRejected();

private:
  Log m_log;
};
