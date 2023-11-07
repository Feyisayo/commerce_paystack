<?php
/**
 * @file
 * Rules Integration for Commerce paystack.
 */

/**
 * Implements hook_rules_action_info().
 */
function commerce_paystack_rules_action_info() {
  $actions = array();

  $actions['commerce_paystack_add_paystack_charges'] = array(
    'label' => t('Calculate and add paystack charge to order'),
    'parameter' => array(
      'commerce_order' => array(
        'type' => 'commerce_order',
        'label' => t('Order to add charge to'),
      ),
    ),
    'group' => t('Commerce Paystack'),
    'callbacks' => array(
      'execute' => 'commerce_paystack_add_paystack_charge',
    ),
  );

  $actions['commerce_paystack_remove_paystack_charges'] = array(
    'label' => t('Remove the paystack charge line item from an order'),
    'parameter' => array(
      'commerce_order' => array(
        'type' => 'commerce_order',
        'label' => t('Order to remove charge from'),
      ),
    ),
    'group' => t('Commerce Paystack'),
    'callbacks' => array(
      'execute' => 'commerce_paystack_remove_paystack_charge',
    ),
  );

  return $actions;
}

/**
 * Rules action callback.
 *
 * Adds the paystack charge line item to an order.
 *
 * @param object $order
 *   The Commerce order.
 */
function commerce_paystack_add_paystack_charge($order) {
  $paystack = commerce_payment_method_instance_load('paystack|commerce_payment_paystack');

  if (!$paystack['settings']['enable_charges']) {
    return;
  }
  // Find and remove the paystack charge line item from the order.
  $paystack_charge_line_item_id = '';
  $order_wrapper = entity_metadata_wrapper('commerce_order', $order);
  foreach ($order_wrapper->commerce_line_items as $delta => $line_item_wrapper) {
    if ('paystack_charge' == $line_item_wrapper->type->value()) {
      $paystack_charge_line_item_id = $line_item_wrapper->line_item_id->value();
      continue;
    }
  }
  if ($paystack_charge_line_item_id) {
    commerce_line_item_delete($paystack_charge_line_item_id);
  }

  // Add the paystack charge line item to the order.
  $paystack_charge = commerce_paystack_calculate_charge($order);
  if ($paystack_charge) {
    $paystack = commerce_payment_method_instance_load('paystack|commerce_payment_paystack');
    $paystack_charge_line_item = commerce_line_item_new('paystack_charge');
    $line_item_types = commerce_line_item_types();
    $paystack_charge_line_item->line_item_label = $line_item_types['paystack_charge']['name'];
    $paystack_charge_line_item->order_id = $order->order_id;
    $paystack_charge_line_item->quantity = 1;
    $currency_code = 'NGN';$paystack['settings']['currency_code'];

    $unit_price = array(
      'amount' => $paystack_charge,
      'currency_code' => $currency_code,
      'data' => array(
        'components' => array(
          array(
            'name' => 'paystack_charge',
            'price' => array(
              'amount' => $paystack_charge,
              'currency_code' => $currency_code,
              'data' => array(),
            ),
            'included' => TRUE,
          ),
        ),
      ),
    );
    $paystack_charge_line_item->commerce_unit_price[LANGUAGE_NONE][] = $unit_price;

    commerce_line_item_save($paystack_charge_line_item);

    $order_wrapper->commerce_line_items[] = $paystack_charge_line_item;
    commerce_order_save($order);
  }
}

/**
 * Rules action callback.
 *
 * Removes the paystack charge line item from an order.
 *
 * @param object $order
 *   The Commerce order.
 */
function commerce_paystack_remove_paystack_charge($order) {
  $order_wrapper = entity_metadata_wrapper('commerce_order', $order);
  foreach ($order_wrapper->commerce_line_items as $delta => $line_item_wrapper) {
    if ('paystack_charge' == $line_item_wrapper->type->value()) {
      commerce_line_item_delete($line_item_wrapper->line_item_id->value());
      commerce_order_save($order);
      return;
    }
  }
}
