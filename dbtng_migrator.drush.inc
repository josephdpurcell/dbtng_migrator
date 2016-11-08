<?php

/**
 * Implementation of drush hook_drush_command().
 */
function dbtng_migrator_drush_command() {
  $items = array();

  $items['dbtng-replicate'] = array(
    'description' => "Replicate database 'origin' across to database 'destination' independant of database driver type.",
    'arguments' => array(
      'origin' => 'The connection key for the database to copy from',
      'destination' => 'The connection key for the database to copy into',
    ),
    'examples' => array(
      'drush dbtng-replicate default pgsql' => 'Replicate the default database to a datase with the key named "pgsql" in $databases in settings.php',
    ),
    'aliases' => array('replicate', 'dbtng-migrator', 'dbtng-migrate'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL, // No bootstrap at all.
  );

  $items['dbtng-check'] = array(
    'description' => "Check replication from 'origin' across to database 'destination'.",
    'arguments' => array(
      'origin' => 'The connection key for the database to copy from',
      'destination' => 'The connection key for the database to copy into',
    ),
    'examples' => array(
      'drush dbtng-check default pgsql' => 'Check replication from the default database to a datase with the key named "pgsql" in $databases in settings.php',
    ),
    'aliases' => array('check', 'dbtng-check'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL, // No bootstrap at all.
  );

  return $items;
}

/**
 * Validation callback to 'dbtng-replicate'.
 */
function drush_dbtng_migrator_dbtng_replicate_validate($origin, $destination) {
  if (DRUPAL_CORE_COMPATIBILITY != '7.x' && !module_exists('dbtng')) {
    drush_set_error("DBTNG Migrator requires either DBTNG module or Drupal 7");
  }
  global $databases;
  if (!isset($databases[$origin])) {
    drush_set_error("Origin database '$origin' is not defined in \$databases in settings.php");
  }
  if (!isset($databases[$destination])) {
    drush_set_error("Destination database '$destination' is not defined in \$databases in settings.php");
  }
  $form_state['values'] = array(
    'migrate_origin' => $origin,
    'migrate_destination' => $destination,
  );

  require_once dirname(__FILE__) . '/dbtng_migrator.admin.inc';

  drupal_form_submit('dbtng_migrator_settings', $form_state);
  if (form_set_error()) {
    return FALSE;
  }
  return TRUE;
}

/**
 * Callback function of command 'dbtng-replicate'.
 */
function drush_dbtng_migrator_dbtng_replicate($origin, $destination) {
}

/**
 * Validation callback to 'dbtng-check'.
 *
 * @param string $origin
 *   The originating database name from settings.php.
 * @param string $destination
 *   The destination database name from settings.php.
 *
 * @return bool
 *   Returns true on success, else false.
 */
function drush_dbtng_migrator_dbtng_check_validate($origin, $destination) {
  if (DRUPAL_CORE_COMPATIBILITY != '7.x' && !module_exists('dbtng')) {
    drush_set_error("DBTNG Migrator requires either DBTNG module or Drupal 7");
  }
  global $databases;
  if (!isset($databases[$origin])) {
    drush_set_error("Origin database '$origin' is not defined in \$databases in settings.php");
  }
  if (!isset($databases[$destination])) {
    drush_set_error("Destination database '$destination' is not defined in \$databases in settings.php");
  }
  $form_state['values'] = array(
    'migrate_origin' => $origin,
    'migrate_destination' => $destination,
  );

  require_once dirname(__FILE__) . '/dbtng_migrator.admin.inc';

  drupal_form_submit('dbtng_migrator_check_form', $form_state);
  if (form_set_error()) {
    return FALSE;
  }
  return TRUE;
}

/**
 * Callback function of command 'dbtng-check'.
 */
function drush_dbtng_migrator_dbtng_check($origin, $destination) {
}
