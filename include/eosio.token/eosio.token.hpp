/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/time.hpp>
#include <eosiolib/transaction.hpp>


#include <string>

namespace eosiosystem {
   class system_contract;
}

namespace eosio {

   using std::string;

   class [[eosio::contract("eosio.token")]] token : public contract {
      public:
         using contract::contract;

         [[eosio::action]]
         void create( name   issuer,
                      asset  maximum_supply);

         [[eosio::action]]
         void issue( name to, asset quantity, string memo );

         [[eosio::action]]
         void retire( asset quantity, string memo );

         [[eosio::action]]
         void transfer( name    from,
                        name    to,
                        asset   quantity,
                        string  memo );

         [[eosio::action]]
         void open( name owner, const symbol& symbol, name ram_payer );

         [[eosio::action]]
         void close( name owner, const symbol& symbol );

	 /* stake related actions */

	 [[eosio::action]]
	 void stake (name _stake_account, asset _staked ) ;
	
	 [[eosio::action]]
	 void unstake (const name _stake_account, asset _unstaked );

	 [[eosio::action]]
	 void refund(const name owner);

	 [[eosio::action]]
	 void sendinvoice(name from, name to, asset invoice_total, uint32_t payment_due, string descr);

	 [[eosio::action]]
	 void payinvoice(name payer, uint64_t invoice_id, asset invoice_total);

	 [[eosio::action]]
	 void rejectinvoice(name payer, uint64_t invoice_id, string reason);


	/* end of stake actions */

         static asset get_supply( name token_contract_account, symbol_code sym_code )
         {
            stats statstable( token_contract_account, sym_code.raw() );
            const auto& st = statstable.get( sym_code.raw() );
            return st.supply;
         }

         static asset get_balance( name token_contract_account, name owner, symbol_code sym_code )
         {
            accounts accountstable( token_contract_account, owner.value );
            const auto& ac = accountstable.get( sym_code.raw() );
            return ac.balance;
         }

      private:
         struct [[eosio::table]] account {
            asset    balance;

            uint64_t primary_key()const { return balance.symbol.code().raw(); }
         };

         struct [[eosio::table]] currency_stats {
            asset    supply;
            asset    max_supply;
            name     issuer;

            uint64_t primary_key()const { return supply.symbol.code().raw(); }
         };

         typedef eosio::multi_index< "accounts"_n, account > accounts;
         typedef eosio::multi_index< "stat"_n, currency_stats > stats;

         void sub_balance( name owner, asset value );
         void add_balance( name owner, asset value, name ram_payer );
	

	 /** stake related */

     	const uint16_t  WEEK_MULTIPLIERX100 = 100;
    	const uint16_t  MONTH_MULTIPLIERX100 = 150;
    	const uint16_t  QUARTER_MULTIPLIERX100 = 200;
    	const int64_t   BASE_WEEKLY = 20000000000;

    	const uint8_t   WEEKLY = 1;
    	const uint8_t   MONTHLY = 2;
    	const uint8_t   QUARTERLY = 3;

	//    const uint32_t  WEEK_WAIT =    (60 * 3);   // TESTING Speed Only
	//    const uint32_t  MONTH_WAIT =   (60 * 12);  // TESTING Speed Only
	//    const uint32_t  QUARTER_WAIT = (60 * 36);  // TESTING Speed Only

    	const uint32_t   WEEK_WAIT =    (60 * 60 * 24 * 7);
    	const uint32_t   MONTH_WAIT =   (60 * 60 * 24 * 7 * 4);
    	const uint32_t   QUARTER_WAIT = (60 * 60 * 24 * 7 * 4 * 3);
    	const uint32_t   TENDAY_WAIT = (60 * 60 * 24 * 10);


    	// @abi table configs i64
    	struct [[eosio::table]] config {
        	uint64_t        config_id;
        	uint8_t         running;
        	name    	overflow;
        	uint32_t        active_accounts;
        	asset           staked_weekly;
        	asset           staked_monthly;
        	asset           staked_quarterly;
        	asset           total_staked;
        	asset           total_escrowed_monthly;
        	asset           total_escrowed_quarterly;
        	uint64_t        total_shares;
        	asset           base_payout;
        	asset           bonus;
        	asset           total_payout;
        	asset           interest_share;
        	asset           unclaimed_tokens;
        	asset           spare_a1;
        	asset           spare_a2;
        	uint64_t        spare_i1;
        	uint64_t        spare_i2;

        	uint64_t    primary_key() const { return config_id; }

        	EOSLIB_SERIALIZE (config, (config_id)(running)(overflow)(active_accounts)(staked_weekly)(staked_monthly)(staked_quarterly)(total_staked)(total_escrowed_monthly)(total_escrowed_quarterly)(total_shares)(base_payout)(bonus)(total_payout)(interest_share)(unclaimed_tokens)
        (spare_a1)(spare_a2)(spare_i1)(spare_i2));
    	};

    	typedef eosio::multi_index<"configs"_n, config> config_table;

    	// @abi table stakes i64
    	struct [[eosio::table]] stake_row {
        	name    	stake_account;
        	uint8_t         stake_period;
        	asset           staked;
        	uint32_t        stake_date;
        	uint32_t        stake_due;
        	asset           escrow;

        	uint64_t        primary_key () const { return stake_account.value; }

        	EOSLIB_SERIALIZE (stake_row, (stake_account)(stake_period)(staked)(stake_date)(stake_due)(escrow));
    	};

   	typedef eosio::multi_index<"stakes"_n, stake_row> stake_table;

	struct [[eosio::table]] lock_balance {
		name            stake_account;
                asset 		locked_balance;
		uint32_t        refund_due;

                //uint64_t primary_key() const { return locked_balance.symbol.code().raw(); }
		uint64_t        primary_key () const { return stake_account.value; }
        };

	typedef multi_index<"lockedbals"_n, lock_balance> lock_balances;


	inline asset get_locked_balance(name account)
        {
                lock_balances lockbalances(_self, account.value);
                auto ac = lockbalances.find(account.value);
                if (ac == lockbalances.end())
                {
                        return asset(0, symbol("BRM", 3));
                }
                return ac->locked_balance;
        }
	
	void unlock_balance(name owner);

	/** start utility payments **/

	const uint8_t   BRM_INVOICE_STATUS_OPEN = 1;
	const uint8_t   BRM_INVOICE_STATUS_PART_PAID = 2;
	const uint8_t   BRM_INVOICE_STATUS_PAID = 3;
	const uint8_t   BRM_INVOICE_STATUS_REJECTED = 4;
	const uint8_t   BRM_INVOICE_STATUS_WRITEOFF = 5;


	//merchant invoice
	struct [[eosio::table]] utility_invoice {
		uint64_t	invoice_id_key;
		uint8_t		invoice_status;
                name            from_account;
                name            to_account;
                asset           invoice_total;
                asset           paid_total;
                uint32_t        payment_due;
                uint32_t        payment_date;
                string          payment_id;
		string		invoice_descr;
		
                uint64_t        primary_key () const { return invoice_id_key; }
        };

	//user received invoice
	struct [[eosio::table]] customer_invoice {
                uint64_t        invoice_id_key;
                uint32_t        created_date;
		name		sender;
                uint64_t        primary_key () const { return invoice_id_key; }
        };

        typedef multi_index<"uinvoices"_n, utility_invoice> uinvoice_table;
        typedef multi_index<"cinvoices"_n, customer_invoice> cinvoice_table;

	void _notify(name invoice_status, const string message, const utility_invoice& d);

};

/* end of stake defs */



} /// namespace eosio
