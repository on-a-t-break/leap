#pragma once
#include <trx_provider.hpp>
#include <string>
#include <vector>
#include <boost/program_options.hpp>
#include <eosio/chain/transaction.hpp>
#include <eosio/chain/asset.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <fc/io/json.hpp>

namespace eosio::testing {

   struct signed_transaction_w_signer {
      signed_transaction_w_signer(eosio::chain::signed_transaction trx, fc::crypto::private_key key) : _trx(std::move(trx)), _signer(key) {}

      eosio::chain::signed_transaction _trx;
      fc::crypto::private_key _signer;
   };

   struct action_pair_w_keys {
      action_pair_w_keys(eosio::chain::action first_action, eosio::chain::action second_action, fc::crypto::private_key first_act_signer, fc::crypto::private_key second_act_signer)
            : _first_act(std::move(first_action)), _second_act(std::move(second_action)), _first_act_priv_key(std::move(first_act_signer)), _second_act_priv_key(std::move(second_act_signer)) {}

      eosio::chain::action _first_act;
      eosio::chain::action _second_act;
      fc::crypto::private_key _first_act_priv_key;
      fc::crypto::private_key _second_act_priv_key;
   };

   struct account_name_generator {
      // This account_name_generator provides the means to generate 12 character account names where the left-most 2 characters are reserved
      // to identify the trx generator.  The right 10 characters are determined based on incrementing through the allowed char_map one at a time
      // before incrementing the next character to the left.
      // The _name_index_vec tracks the index into the char_map for each of the 12 characters of the name.
      // For example:
      // Transaction Generators would create new account names as follows:
      // generator ID: 5      generator ID: 41
      // 1a1111111111          2f1111111111
      // 1a1111111112          2f1111111112
      // 1a1111111113          2f1111111113
      // 1a1111111114          2f1111111114
      // 1a1111111115          2f1111111115
      // ...                   ...
      // 1a111111111z          2f111111111z
      // 1a1111111121          2f1111111121
      // 1a1111111122          2f1111111122
      account_name_generator() : _name_index_vec(acct_name_len, 0) {}

      static constexpr char char_map[] = "12345abcdefghijklmnopqrstuvwxyz";
      static constexpr int acct_name_char_cnt = sizeof(char_map) - 1;
      const int acct_name_len = 12;

      // Reserving the first 2 characters in the 12 char account name to identify the transaction generator sending the trx to create the new account.
      // So 31 ^ 2 gives 961 (so w/0 based index, 960) since more than 31 generators may be desired, but > 961 is likely unwarranted.
      // This provides an easy way to deduplicate the names being generated by the parallel trx generators.
      const int prefix_max = 960;
      std::vector<int> _name_index_vec;

      void increment() {
         increment(_name_index_vec.size() - 1);
      }

      void incrementPrefix() {
         increment(1);
      }

      void setPrefix(int generator_id) {
         if (generator_id > prefix_max) {
            elog("Account Name Generator Prefix above allowable ${max}", ("max", prefix_max));
            return;
         }
         _name_index_vec[0] = 0;
         _name_index_vec[1] = 0;
         for(int i = 0; i < generator_id; i++) {
            incrementPrefix();
         }
      };

      std::string calc_name() {
         std::string name;
         name.reserve(12);
         for(auto i: _name_index_vec) {
            name += char_map[i];
         }
         return name;
      }

      private:
         void increment(int index) {
            _name_index_vec[index]++;
            if(_name_index_vec[index] >= acct_name_char_cnt) {
               _name_index_vec[index] = 0;
               increment(index - 1);
            }
         }
   };

   struct trx_generator_base_config {
      uint16_t _generator_id = 0;
      eosio::chain::chain_id_type _chain_id = eosio::chain::chain_id_type::empty_chain_id();
      eosio::chain::name _contract_owner_account = eosio::chain::name();
      fc::microseconds _trx_expiration_us = fc::seconds(3600);
      eosio::chain::block_id_type _last_irr_block_id = eosio::chain::block_id_type();
      std::string _log_dir = ".";
      bool _stop_on_trx_failed = true;

      std::string to_string() const {
         std::ostringstream ss;
         ss << " generator id: " << _generator_id << " chain id: " << std::string(_chain_id) << " contract owner account: " 
            << _contract_owner_account << " trx expiration seconds: " << _trx_expiration_us.to_seconds() << " lib id: " << std::string(_last_irr_block_id)
            << " log dir: " << _log_dir << " stop on trx failed: " << _stop_on_trx_failed;
         return std::move(ss).str();
      };
   };

   struct user_specified_trx_config {
      std::string _abi_data_file_path;
      std::string _actions_data_json_file_or_str;
      std::string _actions_auths_json_file_or_str;

      bool fully_configured() const {
         return !_abi_data_file_path.empty() && !_actions_data_json_file_or_str.empty() && !_actions_auths_json_file_or_str.empty();
      }

      bool partially_configured() const {
         return !fully_configured() && (!_abi_data_file_path.empty() || !_actions_data_json_file_or_str.empty() || !_actions_auths_json_file_or_str.empty());
      }

      std::string to_string() const {
         std::ostringstream ss;
         ss << "User Transaction Specified: Abi File: " << _abi_data_file_path << " Actions Data: " << _actions_data_json_file_or_str << " Actions Auths: " << _actions_auths_json_file_or_str;
         return std::move(ss).str();
      };
   };

   struct accounts_config {
      std::vector<eosio::chain::name> _acct_name_vec;
      std::vector<fc::crypto::private_key> _priv_keys_vec;

      std::string to_string() const {
         std::ostringstream ss;
         ss << "Accounts Specified: accounts: [ ";
         for(size_t i = 0; i < _acct_name_vec.size(); ++i) {
               ss << _acct_name_vec.at(i);
               if(i < _acct_name_vec.size() - 1) {
                  ss << ", ";
               }
         }
         ss << " ] keys: [ ";
         for(size_t i = 0; i < _priv_keys_vec.size(); ++i) {
               ss << _priv_keys_vec.at(i).to_string();
               if(i < _priv_keys_vec.size() - 1) {
                  ss << ", ";
               }
         }
         ss << " ]";
         return std::move(ss).str();
      };
   };

   struct trx_generator_base {
      const trx_generator_base_config& _config;
      trx_provider _provider;

      uint64_t _total_us = 0;
      uint64_t _txcount = 0;

      std::vector<signed_transaction_w_signer> _trxs;
      std::vector<action_pair_w_keys> _action_pairs_vector;

      uint64_t _nonce = 0;
      uint64_t _nonce_prefix = 0;


      trx_generator_base(const trx_generator_base_config& trx_gen_base_config, const provider_base_config& provider_config);

      virtual ~trx_generator_base() = default;

      virtual void update_resign_transaction(eosio::chain::signed_transaction& trx, const fc::crypto::private_key& priv_key, uint64_t& nonce_prefix, uint64_t& nonce,
                                     const fc::microseconds& trx_expiration, const eosio::chain::chain_id_type& chain_id, const eosio::chain::block_id_type& last_irr_block_id);

      void push_transaction(signed_transaction_w_signer& trx, uint64_t& nonce_prefix,
                            uint64_t& nonce, const fc::microseconds& trx_expiration, const eosio::chain::chain_id_type& chain_id,
                            const eosio::chain::block_id_type& last_irr_block_id);

      void set_transaction_headers(eosio::chain::transaction& trx, const eosio::chain::block_id_type& last_irr_block_id, const fc::microseconds& expiration, uint32_t delay_sec = 0);

      signed_transaction_w_signer create_trx_w_actions_and_signer(std::vector<eosio::chain::action>&& act, const fc::crypto::private_key& priv_key, uint64_t& nonce_prefix,
                                                                  uint64_t& nonce, const fc::microseconds& trx_expiration, const eosio::chain::chain_id_type& chain_id,
                                                                  const eosio::chain::block_id_type& last_irr_block_id);

      void log_first_trx(const std::string& log_dir, const eosio::chain::signed_transaction& trx);

      bool generate_and_send();
      bool tear_down();
      void stop_generation();
      bool stop_on_trx_fail();
   };

   struct transfer_trx_generator : public trx_generator_base {
      accounts_config _accts_config;

      transfer_trx_generator(const trx_generator_base_config& trx_gen_base_config, const provider_base_config& provider_config, const accounts_config& accts_config);

      void create_initial_transfer_transactions(uint64_t& nonce_prefix, uint64_t& nonce);
      eosio::chain::bytes make_transfer_data(const eosio::chain::name& from, const eosio::chain::name& to, const eosio::chain::asset& quantity, const std::string& memo);
      auto make_transfer_action(eosio::chain::name account, eosio::chain::name from, eosio::chain::name to, eosio::chain::asset quantity, std::string memo);
      void create_initial_transfer_actions(const std::string& salt, const uint64_t& period);

      bool setup();
   };

   void locate_key_words_in_action_mvo(std::vector<std::string>& acct_gen_fields_out, const fc::mutable_variant_object& action_mvo, const std::string& key_word);
   void locate_key_words_in_action_array(std::map<int, std::vector<std::string>>& acct_gen_fields_out, const fc::variants& action_array, const std::string& key_word);
   void update_key_word_fields_in_sub_action(const std::string& key, fc::mutable_variant_object& action_mvo, const std::string& action_inner_key, const std::string& key_word);
   void update_key_word_fields_in_action(std::vector<std::string>& acct_gen_fields, fc::mutable_variant_object& action_mvo, const std::string& key_word);
   fc::variant json_from_file_or_string(const std::string& file_or_str, fc::json::parse_type ptype = fc::json::parse_type::legacy_parser);

   struct trx_generator : public trx_generator_base{
      user_specified_trx_config _usr_trx_config;
      account_name_generator _acct_name_generator;

      eosio::chain::abi_serializer _abi;
      std::vector<fc::mutable_variant_object> _unpacked_actions;
      std::map<int, std::vector<std::string>> _acct_gen_fields;

      const fc::microseconds abi_serializer_max_time = fc::seconds(10); // No risk to client side serialization taking a long time

      trx_generator(const trx_generator_base_config& trx_gen_base_config, const provider_base_config& provider_config, const user_specified_trx_config& usr_trx_config);


      std::vector<eosio::chain::action> generate_actions();
      virtual void update_resign_transaction(eosio::chain::signed_transaction& trx, const fc::crypto::private_key& priv_key, uint64_t& nonce_prefix, uint64_t& nonce,
                                     const fc::microseconds& trx_expiration, const eosio::chain::chain_id_type& chain_id, const eosio::chain::block_id_type& last_irr_block_id);


      bool setup();
   };
}
