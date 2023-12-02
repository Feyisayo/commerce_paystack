<?php

/**
 * @file
 * Contains Commerce Payment callback functions for commerce_paystack.
 */

/**
 * Default settings for PayStack.
 *
 * Dummy settings, really.
 */
function commerce_paystack_default_settings() {
  return array(
    'mode' => 'inline',
    'secret_key' => 'SECRET_KEY',
    'public_key' => 'PUBLIC_KEY',
    'currency_code' => 'NGN',
  );
}

/**
 * Payment method callback: redirect form.
 */
function commerce_paystack_redirect_form($form, &$form_state, $order, $payment_method) {
  if (empty($payment_method['settings']['secret_key']) ||
    empty($payment_method['settings']['public_key']) ||
    empty($payment_method['settings']['currency_code'])) {

    drupal_set_message(t('PayStack has not been properly configured'), 'error');
    watchdog('commerce_paystack', 'Payment attempted without properly configuring PayStack');
    return array();
  }

  global $user;

  // Abort if there is no email associated with the order.
  if (!isset($order->mail) || !$order->mail) {
    drupal_set_message(t('An email address needs to be associated with this order'), 'error');
    watchdog('commerce_paystack', 'Email address not associated with order ' . $order->order_number);
    return array();
  }

  // Create a new payment transaction to represent this payment attempt.
  $wrapper = entity_metadata_wrapper('commerce_order', $order);
  $currency_code = $payment_method['settings']['currency_code'];
  $order_currency_code = $wrapper->commerce_order_total->currency_code->value();
  $amount = commerce_currency_amount_to_decimal(
    commerce_currency_convert($wrapper->commerce_order_total->amount->value(), $order_currency_code, $currency_code),
    $currency_code
  );
  $transaction = commerce_payment_transaction_new(
    $payment_method['method_id'],
    $order->order_id
  );
  $transaction->instance_id = $payment_method['method_id'];
  $transaction->status = COMMERCE_PAYMENT_STATUS_PENDING;
  $transaction->amount = $amount * 100;
  $transaction->currency_code = $currency_code;
  commerce_payment_transaction_save($transaction);

  $_SESSION['commerce_paystack_transaction_id'] = $transaction->transaction_id;

  $callback_url = url('checkout/' . $order->order_id . '/payment/return/' . $order->data['payment_redirect_key'], array('absolute' => TRUE));

  // Get billing details ready for Paystack.
  $custom_fields = array();
  $custom_fields_str = '';
  if (isset($wrapper->commerce_customer_billing) && isset($wrapper->commerce_customer_billing->commerce_customer_address)) {
    $billing_info = $wrapper->commerce_customer_billing->commerce_customer_address->value();

    if ($billing_info['first_name']) {
      $custom_fields[] = array(
        'display_name' => 'Billing First Name',
        'variable_name' => "first_name",
        'value' => $billing_info['first_name'],
      );
      $custom_fields_str .= "{
        display_name: 'Billing First Name',
        variable_name: 'first_name',
        value: '{$billing_info['first_name']}'
       },";

    }
    if ($billing_info['last_name']) {
      $custom_fields[] = array(
        'display_name' => 'Billing Surname',
        'variable_name' => "last_name",
        'value' => $billing_info['last_name'],
      );
      $custom_fields_str .= "{
        display_name: 'Billing Surname',
        variable_name: 'last_name',
        value: '{$billing_info['last_name']}'
       },";
    }
  }

  commerce_paystack_create_customer($order->mail);

  // Create the checkout form based on the standard or inline mode.
  if ($payment_method['settings']['mode'] == 'standard') {
    // Initialize the payment transaction from PayStack.
    $url = 'https://api.paystack.co/transaction/initialize';
    $options = array();
    $options['method'] = 'POST';
    $data = array(
      'amount' => $transaction->amount,
      'email' => $order->mail,
      'callback_url' => $callback_url,
    );
    if ($custom_fields) {
      $data['metadata']['custom_fields'] = $custom_fields;
    }
    $options['data'] = json_encode($data);

    $options['headers'] = array(
      'authorization' => 'Bearer ' . $payment_method['settings']['secret_key'],
      'content-type' => 'application/json',
      'cache-control' => 'no-cache',
    );

    $response = drupal_http_request($url, $options);
    if (property_exists($response, 'error')) {
      drupal_set_message(t('An unexpected error occurred connecting to PayStack. Please try again later'), 'error');
      watchdog('commerce_paystack', 'Error initializing payment: @response', array('@response' => print_r($response, TRUE)));
      return array();
    }

    $paystack_txn = json_decode($response->data, TRUE);

    // Save the Paystack reference. It will come in handy if the user does not
    // return as expected.
    $transaction->remote_id = $paystack_txn['data']['reference'];
    commerce_payment_transaction_save($transaction);

    // Redirect the user to PayStack using the returned authorization URL.
    $form['#action'] = $paystack_txn['data']['authorization_url'];

    // Add the redirection behaviour that would have been automatically added
    // if 'offsite_autoredirect' had been set to TRUE.
    $form['#attached']['js'][] = drupal_get_path('module', 'commerce_payment') . '/commerce_payment.js';
    $form['help']['#markup'] = '<div class="checkout-help">' . t('Please wait while you are redirected to the payment server. If nothing happens within 10 seconds, please click on the button below.') . '</div>';

    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Proceed to PayStack'),
    );

  }
  else {

    drupal_add_js('https://js.paystack.co/v1/inline.js', 'external');

    $form['#action'] = $callback_url;

    $form['pay'] = array(
      '#markup' => "<a href='#' class='paystack-btn' onclick='payWithPaystack()'>Pay with PayStack</a>",
      '#weight' => 500,
    );

    drupal_add_css(
      drupal_get_path('module', 'commerce_paystack') . '/commerce_paystack.css',
      'file'
    );

    $script = <<<SCRIPT
    jQuery(document).ready(
      function payWithPaystack(){
        var handler = PaystackPop.setup({
          key: '{$payment_method['settings']['public_key']}',
          email: '{$order->mail}',
          amount: {$transaction->amount},
          ref: "{$transaction->transaction_id}",
          metadata: {
            custom_fields: [
              $custom_fields_str
            ]
          },
          callback: function(response){
            window.location.href = '$callback_url' + '/' + response.reference;
          },
          onClose: function(){
            window.location.href = '$callback_url';
          }
        });
        handler.openIframe();
      })
SCRIPT;
    drupal_add_js(
      $script,
      array('type' => 'inline', 'scope' => 'footer', 'weight' => 5)
    );
  }

  return $form;
}

/**
 * Payment method callback: redirect form return validation.
 */
function commerce_paystack_redirect_form_validate($order, $payment_method) {
  // Ensure the transaction is present.
  if (!isset($_SESSION['commerce_paystack_transaction_id'])) {
    return FALSE;
  }
  elseif ($payment_method['settings']['mode'] == 'inline') {
    // If the inline mode is used make sure the PayStack references exists in
    // the URL.
    if (arg(5) == NULL) {
      watchdog('commerce_paystack', 'PayStack reference not found in URL for using inline mode for payment transaction @id', array('@id' => $_SESSION['commerce_paystack_transaction_id']), WATCHDOG_WARNING);
      return FALSE;
    }
  }

  return TRUE;
}

/**
 * Payment method callback: redirect form submission.
 */
function commerce_paystack_redirect_form_submit($order, $payment_method) {
  $txn_ref = $_SESSION['commerce_paystack_transaction_id'];

  // If the inline mode was used then the payment transaction needs to be
  // updated with a remote id ie the PayStack reference.
  if ($payment_method['settings']['mode'] == 'inline') {
    $transaction = commerce_payment_transaction_load($txn_ref);
    $transaction->remote_id = arg(5);
    commerce_payment_transaction_save($transaction);
  }

  // Look up transaction from PayStack.
  $txn_info = commerce_paystack_update_payment($txn_ref);
  if (!$txn_info) {
    drupal_set_message(t('There was an unexpected error looking up the your transaction details from PayStack'), 'error');
    watchdog('commerce_paystack', 'Error looking up transaction ID @txnref on PayStack', array('@txnref' => $txn_ref), WATCHDOG_ALERT);

    return;
  }

  unset($_SESSION['commerce_paystack_transaction_id']);
}

/**
 * Creates/updates a customer using the current user's names
 * For GAN only at the moment
 */
function commerce_paystack_create_customer ($user_email) {
  $paystack = commerce_payment_method_instance_load('paystack|commerce_payment_paystack');
  $headers = array(
    'authorization' => 'Bearer ' . $paystack['settings']['secret_key'],
    'content-type' => 'application/json',
    'cache-control' => 'no-cache',
  );
  // Fetch the customer
  watchdog('commerce_paystack', 'Checking for customer profile of @email on Paystack', array('@email' => $user_email));
  $url = 'https://api.paystack.co/customer/' . $user_email;
  $options = array();
  $options['method'] = 'GET';
  $options['headers'] = $headers;
  $response = drupal_http_request($url, $options);
  if (property_exists($response, 'error')) {
    watchdog('commerce_paystack', 'Error fetching customer details from Paystack: @response', array('@response' => print_r($response, TRUE)));
    return FALSE;
  }

  $customer_info = json_decode($response->data, TRUE);
  if ($customer_info['status'] == True && $customer_info['data']['first_name']) {
    watchdog('commerce_paystack', 'Customer profile for @email found on Paystack', array('@email' => $user_email));
    return TRUE;
  }

  // Create/update customer as he/she does not exist or names are missing
  $user_wrapper = entity_metadata_wrapper('user', user_load_by_mail($user_email));
  $data = array(
    'email' => $user_email,
    'first_name' => ucfirst(strtolower( $user_wrapper->field_first_name->value() )),
    'last_name' => ucfirst(strtolower( $user_wrapper->field_surname->value() ))
  );
  $url = 'https://api.paystack.co/customer';
  $options = array();
  $options['method'] = 'POST';
  $options['data'] = json_encode($data);
  $options['headers'] = $headers;

  watchdog('commerce_paystack', 'Creating customer on Paystack using @data', array('@data' => print_r($data, TRUE)));
  $response = drupal_http_request($url, $options);

  if (property_exists($response, 'error')) {
    watchdog('commerce_paystack', 'Error creating customer on Paystack: @response', array('@response' => print_r($response, TRUE)));
  } else {
    watchdog('commerce_paystack', 'Customer created on Paystack: @response', array('@response' => print_r($response, TRUE)));
  }
}