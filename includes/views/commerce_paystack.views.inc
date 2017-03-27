<?php

/**
 * @file
 * Views integration for Commerce PayStack.
 */

/**
 * Implements hook_views_data_alter().
 */
function commerce_paystack_views_data_alter(&$data) {
  $data['commerce_payment_transaction']['get_status'] = array(
    'field' => array(
      'title' => t('Update from PayStack'),
      'help' => t('Get the status of the payment from PayStack'),
      'handler' => 'commerce_paystack_field_handler_get_status',
    ),
  );
}
