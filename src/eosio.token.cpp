/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <eosio.token/eosio.token.hpp>

namespace eosio {

void token::create( name   issuer,
                    asset  maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( maximum_supply.is_valid(), "invalid supply");
    eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    eosio_assert( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}


void token::issue( name to, asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( st.issuer );
    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must issue positive quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st.issuer );

    if( to != st.issuer ) {
      SEND_INLINE_ACTION( *this, transfer, { {st.issuer, "active"_n} },
                          { st.issuer, to, quantity, memo }
      );
    }
}

void token::retire( asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.issuer );
    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must retire positive quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply -= quantity;
    });

    sub_balance( st.issuer, quantity );
}

void token::transfer( name    from,
                      name    to,
                      asset   quantity,
                      string  memo )
{
    eosio_assert( from != to, "cannot transfer to self" );
    require_auth( from );
    eosio_assert( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable( _self, sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );

    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    auto payer = has_auth( to ) ? to : from;

    sub_balance( from, quantity );
    add_balance( to, quantity, payer );
}

void token::sub_balance( name owner, asset value ) {
   accounts from_acnts( _self, owner.value );

   const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
   eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );

   from_acnts.modify( from, owner, [&]( auto& a ) {
         a.balance -= value;
      });
}

void token::add_balance( name owner, asset value, name ram_payer )
{
   accounts to_acnts( _self, owner.value );
   auto to = to_acnts.find( value.symbol.code().raw() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

void token::open( name owner, const symbol& symbol, name ram_payer )
{
   require_auth( ram_payer );

   auto sym_code_raw = symbol.code().raw();

   stats statstable( _self, sym_code_raw );
   const auto& st = statstable.get( sym_code_raw, "symbol does not exist" );
   eosio_assert( st.supply.symbol == symbol, "symbol precision mismatch" );

   accounts acnts( _self, owner.value );
   auto it = acnts.find( sym_code_raw );
   if( it == acnts.end() ) {
      acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = asset{0, symbol};
      });
   }
}

void token::close( name owner, const symbol& symbol )
{
   require_auth( owner );
   accounts acnts( _self, owner.value );
   auto it = acnts.find( symbol.code().raw() );
   eosio_assert( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
   eosio_assert( it->balance.amount == 0, "Cannot close because the balance is not zero." );
   acnts.erase( it );
}


/** BRM staking fucntions **/


void token::stake(name _stake_account, asset _staked)
{
    require_auth(_stake_account);
    uint8_t _stake_period = 1;
    config_table c_t (_self, _self.value);
    auto c_itr = c_t.find(0);
    const auto& c_it = *c_itr;
    stake_table s_t(_self, _self.value);
    //eosio_assert(c_itr->running != 0,"staking is currently disabled.");
    eosio_assert(is_account(_stake_account), "to account does not exist");
    auto sym = _staked.symbol.code();
    stats statstable(_self, sym.raw());
    const auto &st = statstable.get(sym.raw());
    eosio_assert(_staked.is_valid(), "invalid quantity");
    eosio_assert(_staked.amount > 0, "must transfer positive quantity");
    eosio_assert(_staked.symbol == st.supply.symbol, "symbol precision mismatch");
    eosio_assert(_stake_period >= 1 && _stake_period <= 3, "Invalid stake period.");
    auto itr = s_t.find(_stake_account.value);
    //eosio_assert(itr == s_t.end(), "Account already has a stake. Must unstake first.");

    sub_balance(_stake_account, _staked);
    //asset locked_balance = get_locked_balance(_stake_account);
    //unlock_balance(_stake_account);
    //asset total_staked = _staked + locked_balance;

    asset setme = _staked;
    setme -= _staked;                                                           // get a zero asset value to plug into the escrow row.

    if (itr == s_t.end()) {
    	s_t.emplace(_stake_account, [&](auto &s) {
        	s.stake_account = _stake_account;
        	s.stake_period = _stake_period;
        	s.staked =  _staked;
        	s.escrow = setme;
        	if(_stake_period == WEEKLY){
          		s.stake_due = now() + WEEK_WAIT;
          		s.stake_date = now()+ WEEK_WAIT;
        	}
        	else if(_stake_period == MONTHLY){
          		s.stake_due = now() + WEEK_WAIT;
          		s.stake_date = now()+ MONTH_WAIT;
        	}
        	else if(_stake_period == QUARTERLY){
          		s.stake_due = now() + WEEK_WAIT;
          		s.stake_date = now()+ QUARTER_WAIT;
        	}
    	});

   } else {
	s_t.modify(itr, _self, [&](auto &s) {
                s.stake_account = _stake_account;
                s.stake_period = _stake_period;
                s.staked +=  _staked;
                s.escrow = setme;
                if(_stake_period == WEEKLY){
                        s.stake_due = now() + WEEK_WAIT;
                        s.stake_date = now()+ WEEK_WAIT;
                }
                else if(_stake_period == MONTHLY){
                        s.stake_due = now() + WEEK_WAIT;
                        s.stake_date = now()+ MONTH_WAIT;
                }
                else if(_stake_period == QUARTERLY){
                        s.stake_due = now() + WEEK_WAIT;
                        s.stake_date = now()+ QUARTER_WAIT;
                }
        });

   }

    if (c_itr == c_t.end() ) {
    	c_t.emplace(_stake_account, [&](auto &c) {
        c.active_accounts += 1;
        c.total_staked.amount += _staked.amount;
        if (_stake_period == WEEKLY) {
          c.staked_weekly.amount += _staked.amount;
        }
        else if (_stake_period == MONTHLY) {
          c.staked_monthly.amount += _staked.amount;
        }
        else if (_stake_period == QUARTERLY) {
          c.staked_quarterly.amount += _staked.amount;
        }
    });

    }else {

	c_t.modify(c_itr, _self, [&](auto &c) {
        c.active_accounts += 1;
        c.total_staked.amount += _staked.amount;
        if (_stake_period == WEEKLY) {
          c.staked_weekly.amount += _staked.amount;
        }
        else if (_stake_period == MONTHLY) {
          c.staked_monthly.amount += _staked.amount;
        }
        else if (_stake_period == QUARTERLY) {
          c.staked_quarterly.amount += _staked.amount;
        }
    });

    }

	
}

/** unstake */

void token::unstake(name _stake_account, asset _unstaked)
{
    stake_table s_t(_self, _self.value);
    auto itr = s_t.find(_stake_account.value);
    eosio_assert(itr != s_t.end(), "No stake for the user.You must stake first");
    require_auth(itr->stake_account);
    //add_balance(itr->stake_account, itr->staked, itr->stake_account);

    config_table c_t(_self, _self.value);
    auto c_itr = c_t.find(0);
    const auto& c_it = *c_itr;
    /*eosio_assert(c_itr->running != 0,"staking contract is currently disabled.");
    print("staked amount was ", itr->staked.amount);
    print("staked account was ", itr->stake_account);
    if (itr->escrow.amount > 0){
      add_balance(_self, itr->escrow, _self);              // return the stored escrow - it was deducted from the contract during payout
    }*/

    eosio_assert(itr->staked >= _unstaked, "You cant unstake more than staked");

    uint8_t remove_stake_account = 0;

    if(_unstaked == itr->staked) {
	remove_stake_account = 1;
    }

    c_t.modify(c_itr, _self, [&](auto &c) {                // bookkeeping on the config table to keep the staked & esrowed amounts correct
    	c.active_accounts -= remove_stake_account;
    	c.total_staked.amount -= _unstaked.amount;
    	if (itr->stake_period == WEEKLY) {
      		c.staked_weekly.amount -= _unstaked.amount;
    	}
    	else if ((itr->stake_period == MONTHLY)) {
      		c.staked_monthly.amount -= _unstaked.amount;
      		c.total_escrowed_monthly.amount -= itr->escrow.amount;
    	}
    	else if ((itr->stake_period == QUARTERLY)) {
      		c.staked_quarterly.amount -= _unstaked.amount;
      		c.total_escrowed_quarterly.amount -=  itr->escrow.amount;
    	}
  });
  //lock

  lock_balances lockbalances(_self, _stake_account.value);
  auto ac = lockbalances.find(_stake_account.value);
  if (ac == lockbalances.end())
  {
  	lockbalances.emplace(_self, [&](auto &account) {
                account.stake_account = itr->stake_account;
                account.locked_balance = _unstaked; //itr->staked;
                account.refund_due = now() + TENDAY_WAIT;
        });
  }
  else
  {
        lockbalances.modify(ac, _self, [&](auto &row) {
                row.locked_balance += _unstaked; //itr->staked;
                row.refund_due = now() + TENDAY_WAIT;
        });
  }

  if(remove_stake_account) {
  	s_t.erase(itr);
  	eosio_assert(itr != s_t.end(), "Stake stat not erased properly");
  }else {

	s_t.modify(itr, _self, [&](auto &s) {
                s.staked -=  _unstaked;
        });

	


  }

}

void token::refund(const name owner) {

   require_auth(owner);	
   lock_balances lockbalances(_self, owner.value);
   auto ac = lockbalances.find(owner.value);
   eosio_assert(ac != lockbalances.end(), "Nothing to refund");
   eosio_assert(ac->refund_due < now(), "You need to wait until lock period is over!");

   lockbalances.erase(ac);
   add_balance(owner, ac->locked_balance, owner);

}

void token::unlock_balance(name owner) {

   //remove from lock

   lock_balances lockbalances(_self, owner.value);
   auto ac = lockbalances.find(owner.value);
   if (ac == lockbalances.end()) {
	return;
   }

   lockbalances.erase(ac);
   eosio_assert(ac != lockbalances.end(), "locked balance not erased properly");

}

/** end of BRM stake functions **/

} /// namespace eosio

EOSIO_DISPATCH( eosio::token, (create)(issue)(transfer)(open)(close)(retire)(stake)(unstake)(refund))
