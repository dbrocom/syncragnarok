// Copyright (c) Athena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef TXT_ONLY

#include "../common/nullpo.h"
#include "../common/showmsg.h"

#include "mail.h"
#include "atcommand.h"
#include "itemdb.h"
#include "clif.h"
#include "pc.h"
#include "log.h"
#include "npc.h"

#include <time.h>
#include <string.h>

void mail_clear(struct map_session_data *sd)
{
	sd->mail.nameid = 0;
	sd->mail.index = 0;
	sd->mail.amount = 0;
	sd->mail.zeny = 0;
	sd->auction.amount = 0;

	return;
}

int mail_removeitem(struct map_session_data *sd, short flag)
{
	nullpo_ret(sd);

	if( sd->mail.amount )
	{
		if (flag)
		{ // Item send
			if(log_config.enable_logs&0x2000)
				log_pick_pc(sd, "E", sd->mail.nameid, -sd->mail.amount, &sd->status.inventory[sd->mail.index], sd->status.inventory[sd->mail.index].serial);

			pc_delitem(sd, sd->mail.index, sd->mail.amount, 1, 0);
		}
		else
			clif_additem(sd, sd->mail.index, sd->mail.amount, 0);
	}

	sd->mail.nameid = 0;
	sd->mail.index = 0;
	sd->mail.amount = 0;
	return 1;
}

int mail_removezeny(struct map_session_data *sd, short flag)
{
	nullpo_ret(sd);

	if (flag && sd->mail.zeny > 0)
	{  //Zeny send
		if(log_config.zeny)
			log_zeny(sd, "E", sd, -sd->mail.zeny);

		sd->status.zeny -= sd->mail.zeny;
	}
	sd->mail.zeny = 0;
	clif_updatestatus(sd, SP_ZENY);

	return 1;
}

unsigned char mail_setitem(struct map_session_data *sd, int idx, int amount)
{
	if( sd->state.secure_items )
	{
		clif_displaymessage(sd->fd, "You can't attach. Blocked with @security");
		return 1;
	}

	if( battle_config.super_woe_enable )
	{
		clif_displaymessage(sd->fd, "Super WoE don't allow send items/zeny with attachments");
		return 1;
	}
	
	if( idx == 0 )
	{ // Zeny Transfer
		if( amount < 0 || !pc_can_give_items(pc_isGM(sd)) )
			return 1;

		if( amount > sd->status.zeny )
			amount = sd->status.zeny;

		sd->mail.zeny = amount;
		// clif_updatestatus(sd, SP_ZENY);
		return 0;
	}
	else
	{ // Item Transfer
		idx -= 2;
		mail_removeitem(sd, 0);

		if( idx < 0 || idx >= MAX_INVENTORY )
			return 1;
		if( amount < 0 || amount > sd->status.inventory[idx].amount )
			return 1;
		if( !pc_candrop(sd, &sd->status.inventory[idx]) )
			return 1;

		sd->mail.index = idx;
		sd->mail.nameid = sd->status.inventory[idx].nameid;
		sd->mail.amount = amount;
		
		return 0;
	}
}

bool mail_setattachment(struct map_session_data *sd, struct mail_message *msg)
{
	int n;
	
	nullpo_retr(false,sd);
	nullpo_retr(false,msg);

	if( sd->mail.zeny < 0 || sd->mail.zeny > sd->status.zeny )
		return false;

	n = sd->mail.index;
	if( sd->mail.amount )
	{
		if( sd->status.inventory[n].nameid != sd->mail.nameid )
			return false;

		if( sd->status.inventory[n].amount < sd->mail.amount )
			return false;

		if( sd->weight > sd->max_weight )
			return false;

		memcpy(&msg->item, &sd->status.inventory[n], sizeof(struct item));
		msg->item.amount = sd->mail.amount;
	}
	else
		memset(&msg->item, 0x00, sizeof(struct item));

	msg->zeny = sd->mail.zeny;

	// Removes the attachment from sender
	mail_removeitem(sd,1);
	mail_removezeny(sd,1);

	return true;
}

void mail_getattachment(struct map_session_data* sd, int zeny, struct item* item)
{
	if( item->nameid > 0 && item->amount > 0 )
	{
		pc_additem(sd, item, item->amount);

		if(log_config.enable_logs&0x2000)
			log_pick_pc(sd, "E", item->nameid, item->amount, item, item->serial);
		if( battle_config.lootevent & 8 ) {
			pc_setglobalreg( sd, "LastLootID", item->nameid ); //Last lootet Item ID
			pc_setglobalreg( sd, "LastLootAmount", item->amount ); //Last looted Item Amount
			npc_event_doall_id( "OnLoot", sd->bl.id );
		}

		clif_Mail_getattachment(sd->fd, 0);
	}

	if( zeny > 0 )
	{  //Zeny recieve
		if(log_config.zeny)
			log_zeny(sd, "E", sd, zeny);
		pc_getzeny(sd, zeny);
	}
}

int mail_openmail(struct map_session_data *sd)
{
	nullpo_ret(sd);

	if( sd->state.storage_flag || sd->state.vending || sd->state.buyingstore || sd->state.trading )
		return 0;

	clif_Mail_window(sd->fd, 0);

	return 1;
}

void mail_deliveryfail(struct map_session_data *sd, struct mail_message *msg)
{
	nullpo_retv(sd);
	nullpo_retv(msg);

	if( msg->item.amount > 0 )
	{
		// Item recieve (due to failure)
		if(log_config.enable_logs&0x2000)
			log_pick_pc(sd, "E", msg->item.nameid, msg->item.amount, &msg->item, msg->item.serial);
		if( battle_config.lootevent & 8 ) {
			pc_setglobalreg( sd, "LastLootID", msg->item.nameid ); //Last lootet Item ID
			pc_setglobalreg( sd, "LastLootAmount", msg->item.amount ); //Last looted Item Amount
			npc_event_doall_id( "OnLoot", sd->bl.id );
		} 

		pc_additem(sd, &msg->item, msg->item.amount);
	}

	if( msg->zeny > 0 )
	{
		//Zeny recieve (due to failure)
		if(log_config.zeny)
			log_zeny(sd, "E", sd, msg->zeny);

		sd->status.zeny += msg->zeny;
		clif_updatestatus(sd, SP_ZENY);
	}
	
	clif_Mail_send(sd->fd, true);
}

// This function only check if the mail operations are valid
bool mail_invalid_operation(struct map_session_data *sd)
{
	if( !map[sd->bl.m].flag.town && pc_isGM(sd) < get_atcommand_level(atcommand_mail) )
	{
		ShowWarning("clif_parse_Mail: char '%s' trying to do invalid mail operations.\n", sd->status.name);
		return true;
	}

	return false;
}

#endif
