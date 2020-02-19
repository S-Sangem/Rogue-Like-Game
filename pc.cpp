#include <cstdlib>
#include <ncurses.h>
#include <cstring>

#include "dungeon.h"
#include "pc.h"
#include "utils.h"
#include "move.h"
#include "path.h"
#include "io.h"
#include "object.h"

uint32_t pc_is_alive(dungeon *d)
{
  return d->PC->alive;
}

void place_pc(dungeon *d)
{
  d->PC->position[dim_y] = rand_range(d->rooms->position[dim_y],
                                     (d->rooms->position[dim_y] +
                                      d->rooms->size[dim_y] - 1));
  d->PC->position[dim_x] = rand_range(d->rooms->position[dim_x],
                                     (d->rooms->position[dim_x] +
                                      d->rooms->size[dim_x] - 1));

  pc_init_known_terrain(d->PC);
  pc_observe_terrain(d->PC, d);
}

void config_pc(dungeon *d)
{
  static dice pc_dice(0, 1, 4);

  d->PC = new pc;

  d->PC->symbol = '@';

  place_pc(d);

  d->PC->speed = PC_SPEED;
  d->PC->alive = 1;
  d->PC->sequence_number = 0;
  d->PC->kills[kill_direct] = d->PC->kills[kill_avenged] = 0;
  d->PC->color.push_back(COLOR_WHITE);
  d->PC->damage = &pc_dice;
  d->PC->name = "Isabella Garcia-Shapiro";

  d->character_map[character_get_y(d->PC)][character_get_x(d->PC)] = d->PC;

  dijkstra(d);
  dijkstra_tunnel(d);
}

uint32_t pc_next_pos(dungeon *d, pair_t dir)
{
  static uint32_t have_seen_corner = 0;
  static uint32_t count = 0;

  dir[dim_y] = dir[dim_x] = 0;

  if (in_corner(d, d->PC)) {
    if (!count) {
      count = 1;
    }
    have_seen_corner = 1;
  }

  /* First, eat anybody standing next to us. */
  if (charxy(d->PC->position[dim_x] - 1, d->PC->position[dim_y] - 1)) {
    dir[dim_y] = -1;
    dir[dim_x] = -1;
  } else if (charxy(d->PC->position[dim_x], d->PC->position[dim_y] - 1)) {
    dir[dim_y] = -1;
  } else if (charxy(d->PC->position[dim_x] + 1, d->PC->position[dim_y] - 1)) {
    dir[dim_y] = -1;
    dir[dim_x] = 1;
  } else if (charxy(d->PC->position[dim_x] - 1, d->PC->position[dim_y])) {
    dir[dim_x] = -1;
  } else if (charxy(d->PC->position[dim_x] + 1, d->PC->position[dim_y])) {
    dir[dim_x] = 1;
  } else if (charxy(d->PC->position[dim_x] - 1, d->PC->position[dim_y] + 1)) {
    dir[dim_y] = 1;
    dir[dim_x] = -1;
  } else if (charxy(d->PC->position[dim_x], d->PC->position[dim_y] + 1)) {
    dir[dim_y] = 1;
  } else if (charxy(d->PC->position[dim_x] + 1, d->PC->position[dim_y] + 1)) {
    dir[dim_y] = 1;
    dir[dim_x] = 1;
  } else if (!have_seen_corner || count < 250) {
    /* Head to a corner and let most of the NPCs kill each other off */
    if (count) {
      count++;
    }
    if (!against_wall(d, d->PC) && ((rand() & 0x111) == 0x111)) {
      dir[dim_x] = (rand() % 3) - 1;
      dir[dim_y] = (rand() % 3) - 1;
    } else {
      dir_nearest_wall(d, d->PC, dir);
    }
  }else {
    /* And after we've been there, let's head toward the center of the map. */
    if (!against_wall(d, d->PC) && ((rand() & 0x111) == 0x111)) {
      dir[dim_x] = (rand() % 3) - 1;
      dir[dim_y] = (rand() % 3) - 1;
    } else {
      dir[dim_x] = ((d->PC->position[dim_x] > DUNGEON_X / 2) ? -1 : 1);
      dir[dim_y] = ((d->PC->position[dim_y] > DUNGEON_Y / 2) ? -1 : 1);
    }
  }

  /* Don't move to an unoccupied location if that places us next to a monster */
  if (!charxy(d->PC->position[dim_x] + dir[dim_x],
              d->PC->position[dim_y] + dir[dim_y]) &&
      ((charxy(d->PC->position[dim_x] + dir[dim_x] - 1,
               d->PC->position[dim_y] + dir[dim_y] - 1) &&
        (charxy(d->PC->position[dim_x] + dir[dim_x] - 1,
                d->PC->position[dim_y] + dir[dim_y] - 1) != d->PC)) ||
       (charxy(d->PC->position[dim_x] + dir[dim_x] - 1,
               d->PC->position[dim_y] + dir[dim_y]) &&
        (charxy(d->PC->position[dim_x] + dir[dim_x] - 1,
                d->PC->position[dim_y] + dir[dim_y]) != d->PC)) ||
       (charxy(d->PC->position[dim_x] + dir[dim_x] - 1,
               d->PC->position[dim_y] + dir[dim_y] + 1) &&
        (charxy(d->PC->position[dim_x] + dir[dim_x] - 1,
                d->PC->position[dim_y] + dir[dim_y] + 1) != d->PC)) ||
       (charxy(d->PC->position[dim_x] + dir[dim_x],
               d->PC->position[dim_y] + dir[dim_y] - 1) &&
        (charxy(d->PC->position[dim_x] + dir[dim_x],
                d->PC->position[dim_y] + dir[dim_y] - 1) != d->PC)) ||
       (charxy(d->PC->position[dim_x] + dir[dim_x],
               d->PC->position[dim_y] + dir[dim_y] + 1) &&
        (charxy(d->PC->position[dim_x] + dir[dim_x],
                d->PC->position[dim_y] + dir[dim_y] + 1) != d->PC)) ||
       (charxy(d->PC->position[dim_x] + dir[dim_x] + 1,
               d->PC->position[dim_y] + dir[dim_y] - 1) &&
        (charxy(d->PC->position[dim_x] + dir[dim_x] + 1,
                d->PC->position[dim_y] + dir[dim_y] - 1) != d->PC)) ||
       (charxy(d->PC->position[dim_x] + dir[dim_x] + 1,
               d->PC->position[dim_y] + dir[dim_y]) &&
        (charxy(d->PC->position[dim_x] + dir[dim_x] + 1,
                d->PC->position[dim_y] + dir[dim_y]) != d->PC)) ||
       (charxy(d->PC->position[dim_x] + dir[dim_x] + 1,
               d->PC->position[dim_y] + dir[dim_y] + 1) &&
        (charxy(d->PC->position[dim_x] + dir[dim_x] + 1,
                d->PC->position[dim_y] + dir[dim_y] + 1) != d->PC)))) {
    dir[dim_x] = dir[dim_y] = 0;
  }

  return 0;
}

uint32_t pc_in_room(dungeon *d, uint32_t room)
{
  if ((room < d->num_rooms)                                     &&
      (d->PC->position[dim_x] >= d->rooms[room].position[dim_x]) &&
      (d->PC->position[dim_x] < (d->rooms[room].position[dim_x] +
                                d->rooms[room].size[dim_x]))    &&
      (d->PC->position[dim_y] >= d->rooms[room].position[dim_y]) &&
      (d->PC->position[dim_y] < (d->rooms[room].position[dim_y] +
                                d->rooms[room].size[dim_y]))) {
    return 1;
  }

  return 0;
}

void pc_learn_terrain(pc *p, pair_t pos, terrain_type ter)
{
  p->known_terrain[pos[dim_y]][pos[dim_x]] = ter;
  p->visible[pos[dim_y]][pos[dim_x]] = 1;
}

void pc_reset_visibility(pc *p)
{
  uint32_t y, x;

  for (y = 0; y < DUNGEON_Y; y++) {
    for (x = 0; x < DUNGEON_X; x++) {
      p->visible[y][x] = 0;
    }
  }
}

terrain_type pc_learned_terrain(pc *p, int16_t y, int16_t x)
{
  if (y < 0 || y >= DUNGEON_Y || x < 0 || x >= DUNGEON_X) {
    io_queue_message("Invalid value to %s: %d, %d", __FUNCTION__, y, x);
  }

  return p->known_terrain[y][x];
}

void pc_init_known_terrain(pc *p)
{
  uint32_t y, x;

  for (y = 0; y < DUNGEON_Y; y++) {
    for (x = 0; x < DUNGEON_X; x++) {
      p->known_terrain[y][x] = ter_unknown;
      p->visible[y][x] = 0;
    }
  }
}

void pc_observe_terrain(pc *p, dungeon *d)
{
  pair_t where;
  int16_t y_min, y_max, x_min, x_max;

  y_min = p->position[dim_y] - PC_VISUAL_RANGE;
  if (y_min < 0) {
    y_min = 0;
  }
  y_max = p->position[dim_y] + PC_VISUAL_RANGE;
  if (y_max > DUNGEON_Y - 1) {
    y_max = DUNGEON_Y - 1;
  }
  x_min = p->position[dim_x] - PC_VISUAL_RANGE;
  if (x_min < 0) {
    x_min = 0;
  }
  x_max = p->position[dim_x] + PC_VISUAL_RANGE;
  if (x_max > DUNGEON_X - 1) {
    x_max = DUNGEON_X - 1;
  }

  for (where[dim_y] = y_min; where[dim_y] <= y_max; where[dim_y]++) {
    where[dim_x] = x_min;
    can_see(d, p->position, where, 1, 1);
    where[dim_x] = x_max;
    can_see(d, p->position, where, 1, 1);
  }
  /* Take one off the x range because we alreay hit the corners above. */
  for (where[dim_x] = x_min - 1; where[dim_x] <= x_max - 1; where[dim_x]++) {
    where[dim_y] = y_min;
    can_see(d, p->position, where, 1, 1);
    where[dim_y] = y_max;
    can_see(d, p->position, where, 1, 1);
  }
}

int32_t is_illuminated(pc *p, int16_t y, int16_t x)
{
  return p->visible[y][x];
}

void pc_see_object(character *the_pc, object *o)
{
  if (o)
  {
    o->has_been_seen();
  }
}

void pc_pick_up(dungeon *d, pair_t pos)
{
  if(d->PC->carry.size() > 10)
  {
    io_queue_message("Inventory Full");
  }
  else
  {
    if(d->objmap[pos[dim_y]][pos[dim_x]]->get_symbol() == '&')
    {
      d->PC->carry.push_back(d->objmap[pos[dim_y]][pos[dim_x]]);
      d->objmap[pos[dim_y]][pos[dim_x]] = d->objmap[pos[dim_y]][pos[dim_x]]->next;
    }
    else
    {
       d->PC->carry.push_back(d->objmap[pos[dim_y]][pos[dim_x]]);
       d->objmap[pos[dim_y]][pos[dim_x]] = NULL;
    }
    io_queue_message("Item aquired");
  }
}

void pc_drop_item(dungeon *d)
{
  if(d->PC->carry.size() == 0)
  {
    io_queue_message("Inventory Empty");
  }
  else
  {
    if(d->objmap[d->PC->position[dim_y]][d->PC->position[dim_x]])
    {
      io_queue_message("Can't do that right now");
    }
    else
    {
      d->objmap[d->PC->position[dim_y]][d->PC->position[dim_x]] = d->PC->carry.back();
      d->PC->carry.pop_back();
    }
  }
}

void pc_update(dungeon_t *d, object *o, int swap)
{
  int32_t base_pc;
  uint32_t number_pc, sides_pc;

  base_pc = (d->PC->damage->get_base() + o->damage.get_base()) * swap;
  number_pc = d->PC->damage->get_number() + (o->damage.get_number() * swap);
  sides_pc = d->PC->damage->get_sides() + (o->damage.get_sides() * swap);

  static dice pc_dice(base_pc, number_pc, sides_pc);
  d->PC->damage = &pc_dice;
  d->PC->speed += (o->speed * swap);

  if(d->PC->speed < 0)
    d->PC->speed = 2;
}

void pc_equip(dungeon *d)
{
  mvprintw(3, 60, "%-40s", "Select equipment to equip");
  io_inventory(d);

  uint16_t to_equip, i, added, carry_slot;
  object_type_t to_add;

  to_equip = getch();

  if(to_equip < d->PC->carry.size()+48 && to_equip >= 48)
  {
    switch(to_equip)
    {
    case '0':
      to_add = d->PC->carry[0]->type;
      carry_slot = 0;
      break;
    case '1':
      to_add = d->PC->carry[1]->type;
      carry_slot = 1;
      break;
    case '2':
      to_add = d->PC->carry[2]->type;
      carry_slot = 2;
      break;
    case '3':
      to_add = d->PC->carry[3]->type;
      carry_slot = 3;
      break;
    case '4':
      to_add = d->PC->carry[4]->type;
      carry_slot = 4;
      break;
    case '5':
      to_add = d->PC->carry[5]->type;
      carry_slot = 5;
      break;
    case '6':
      to_add = d->PC->carry[6]->type;
      carry_slot = 6;
      break;
    case '7':
      to_add = d->PC->carry[7]->type;
      carry_slot = 7;
      break;
    case '8':
      to_add = d->PC->carry[8]->type;
      carry_slot = 8;
      break;
    case '9':
      to_add = d->PC->carry[9]->type;
      carry_slot = 9;
      break;
    }

    if(d->PC->equip.size() == 0)
    {
      d->PC->equip.push_back(d->PC->carry[carry_slot]);
      d->PC->carry.erase(d->PC->carry.begin() + carry_slot);

      pc_update(d, d->PC->equip[0], 1);
    }
    else
    {
      for(i = 0; i < d->PC->equip.size(); i++)
      {
      	if(to_add == d->PC->equip[i]->type)
        {
      	  object* temp;
      	  temp = d->PC->equip[i];

      	  d->PC->equip[i] = d->PC->carry[carry_slot];
      	  d->PC->carry[carry_slot] = temp;

      	  pc_update(d, d->PC->equip[0], -1);
      	  added = 1;
      	}
      }
      if(added != 1)
      {
      	d->PC->equip.push_back(d->PC->carry[carry_slot]);
      	d->PC->carry.erase(d->PC->carry.begin() + carry_slot);

      	pc_update(d, d->PC->equip[d->PC->equip.size() - 1], 1);
      }
    }
  }
  refresh();
  getch();
}

void pc_take_off(dungeon *d)
{
  mvprintw(3, 60, "%-40s", "Select equipment to drop");
  io_equipment(d);

  uint16_t to_remove;

  to_remove = getch();
  if(to_remove < d->PC->equip.size()+48 && to_remove >= 48 && d->PC->carry.size() < 10)
  {
    switch(to_remove)
    {
    case '0':
      d->PC->carry.push_back(d->PC->equip[0]);
      pc_update(d, d->PC->equip[0], -1);
      d->PC->equip.erase(d->PC->equip.begin());
      break;
    case '1':
      d->PC->carry.push_back(d->PC->equip[1]);
      pc_update(d, d->PC->equip[1], -1);
      d->PC->equip.erase(d->PC->equip.begin() + 1);
      break;
    case '2':
      d->PC->carry.push_back(d->PC->equip[2]);
      pc_update(d, d->PC->equip[2], -1);
      d->PC->equip.erase(d->PC->equip.begin() + 2);
      break;
    case '3':
      d->PC->carry.push_back(d->PC->equip[3]);
      pc_update(d, d->PC->equip[3], -1);
      d->PC->equip.erase(d->PC->equip.begin() + 3);
      break;
    case '4':
      d->PC->carry.push_back(d->PC->equip[4]);
      pc_update(d, d->PC->equip[4], -1);
      d->PC->equip.erase(d->PC->equip.begin() + 4);
      break;
    case '5':
      d->PC->carry.push_back(d->PC->equip[5]);
      pc_update(d, d->PC->equip[5], -1);
      d->PC->equip.erase(d->PC->equip.begin() + 5);
      break;
    case '6':
      d->PC->carry.push_back(d->PC->equip[6]);
      pc_update(d, d->PC->equip[6], -1);
      d->PC->equip.erase(d->PC->equip.begin() + 6);
      break;
    case '7':
      d->PC->carry.push_back(d->PC->equip[7]);
      pc_update(d, d->PC->equip[7], -1);
      d->PC->equip.erase(d->PC->equip.begin() + 7);
      break;
    case '8':
      d->PC->carry.push_back(d->PC->equip[8]);
      pc_update(d, d->PC->equip[8], -1);
      d->PC->equip.erase(d->PC->equip.begin() + 8);
      break;
    case '9':
      d->PC->carry.push_back(d->PC->equip[9]);
      pc_update(d, d->PC->equip[9], -1);
      d->PC->equip.erase(d->PC->equip.begin() + 9);
      break;
    }
  }
  refresh();
  getch();
}
