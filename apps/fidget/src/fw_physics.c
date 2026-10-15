////////////////////////////////
//~ Playground physics: balls (gravity, walls, ball-ball collisions, mouse repel) + confetti particles
//
// Positions are stored in arena-local pixels (relative to the arena box's top-left) so they survive resizes.

internal Vec4F32
fw_random_color(F32 sat, F32 val)
{
  return fw_hsv_color(fw_rand01(), sat, val, 1.f);
}

internal void
fw_spawn_ball(Vec2F32 p, Vec2F32 v)
{
  int slot = -1;
  for EachIndex(i, BALL_MAX) { if(!app.balls[i].alive) { slot = (int)i; break; } }
  if(slot < 0) { slot = (int)(fw_rand_u64() % BALL_MAX); }
  Ball *b = &app.balls[slot];
  b->alive = 1;
  b->p = p;
  b->v = v;
  b->r = fw_rand_range(7.f, 17.f) * app.scale;
  b->c = fw_random_color(0.65f, 1.f);
}

internal void
fw_spawn_balls_random(U64 count, Vec2F32 dim)
{
  for EachIndex(i, count)
  {
    fw_spawn_ball(v2f32(fw_rand_range(dim.x*0.1f, dim.x*0.9f), fw_rand_range(dim.y*0.05f, dim.y*0.4f)),
                  v2f32(fw_rand_range(-300.f, 300.f) * app.scale, fw_rand_range(-200.f, 100.f) * app.scale));
  }
  app.phys_run = 1;
}

internal void
fw_spawn_confetti(Vec2F32 origin, U64 count)
{
  U64 spawned = 0;
  for EachIndex(i, PARTICLE_MAX)
  {
    if(spawned >= count) { break; }
    Particle *p = &app.parts[i];
    if(p->alive) { continue; }
    F32 ang = fw_rand_range(-3.14159f*0.95f, -3.14159f*0.05f);
    F32 spd = fw_rand_range(250.f, 900.f) * app.scale;
    p->alive = 1;
    p->p = origin;
    p->v = v2f32(cosf(ang) * spd, sinf(ang) * spd);
    p->max_life = p->life = fw_rand_range(1.1f, 2.3f);
    p->size = fw_rand_range(4.f, 9.f) * app.scale;
    p->c = fw_random_color(0.75f, 1.f);
    spawned += 1;
  }
}

internal B32
fw_physics_active(void)
{
  if(app.phys_run) { for EachIndex(i, BALL_MAX) { if(app.balls[i].alive) { return 1; } } }
  for EachIndex(i, PARTICLE_MAX) { if(app.parts[i].alive) { return 1; } }
  return 0;
}

internal void
fw_physics_step(Vec2F32 dim, Vec2F32 mouse, B32 mouse_in, F32 dt)
{
  F32 g = app.gravity * 900.f * app.scale;
  F32 repel_r = 130.f * app.scale;
  if(app.phys_run)
  {
    for EachIndex(i, BALL_MAX)
    {
      Ball *b = &app.balls[i];
      if(!b->alive) { continue; }
      b->v.y += g * dt;
      if(app.repel && mouse_in)
      {
        Vec2F32 d = sub_2f32(b->p, mouse);
        F32 dist = length_2f32(d);
        if(dist < repel_r && dist > 0.01f)
        {
          F32 k = (1.f - dist/repel_r) * 5200.f * app.scale * dt / dist;
          b->v.x += d.x * k; b->v.y += d.y * k;
        }
      }
      b->v = scale_2f32(b->v, 1.f - 0.15f*dt);
      b->p.x += b->v.x * dt;
      b->p.y += b->v.y * dt;
      if(b->p.x < b->r)         { b->p.x = b->r;         b->v.x = -b->v.x * app.bounce; }
      if(b->p.x > dim.x - b->r) { b->p.x = dim.x - b->r; b->v.x = -b->v.x * app.bounce; }
      if(b->p.y < b->r)         { b->p.y = b->r;         b->v.y = -b->v.y * app.bounce; }
      if(b->p.y > dim.y - b->r)
      {
        b->p.y = dim.y - b->r;
        b->v.y = -b->v.y * app.bounce;
        b->v.x *= (1.f - 2.f*dt);
        if(abs_f32(b->v.y) < 14.f * app.scale) { b->v.y = 0; }
      }
    }
    // ball-ball collisions (O(n^2), n <= 300)
    for EachIndex(i, BALL_MAX)
    {
      Ball *a = &app.balls[i];
      if(!a->alive) { continue; }
      for(U64 j = i + 1; j < BALL_MAX; j += 1)
      {
        Ball *b = &app.balls[j];
        if(!b->alive) { continue; }
        Vec2F32 d = sub_2f32(b->p, a->p);
        F32 min_d = a->r + b->r;
        F32 dd = d.x*d.x + d.y*d.y;
        if(dd >= min_d*min_d || dd < 0.0001f) { continue; }
        F32 dist = sqrt_f32(dd);
        Vec2F32 n = v2f32(d.x/dist, d.y/dist);
        F32 overlap = (min_d - dist) * 0.5f;
        a->p.x -= n.x*overlap; a->p.y -= n.y*overlap;
        b->p.x += n.x*overlap; b->p.y += n.y*overlap;
        F32 rel = (b->v.x - a->v.x)*n.x + (b->v.y - a->v.y)*n.y;
        if(rel < 0)
        {
          F32 imp = -(1.f + app.bounce) * rel * 0.5f;
          a->v.x -= n.x*imp; a->v.y -= n.y*imp;
          b->v.x += n.x*imp; b->v.y += n.y*imp;
        }
      }
    }
  }
  for EachIndex(i, PARTICLE_MAX)
  {
    Particle *p = &app.parts[i];
    if(!p->alive) { continue; }
    p->life -= dt;
    if(p->life <= 0) { p->alive = 0; continue; }
    p->v.y += 1500.f * app.scale * dt;
    p->v = scale_2f32(p->v, 1.f - 1.4f*dt);
    p->p.x += p->v.x * dt;
    p->p.y += p->v.y * dt;
  }
}

internal UI_BOX_CUSTOM_DRAW(fw_arena_draw)
{
  Vec2F32 o = box->rect.p0;
  for EachIndex(i, BALL_MAX)
  {
    Ball *b = &app.balls[i];
    if(!b->alive) { continue; }
    Vec2F32 c = add_2f32(o, b->p);
    Vec4F32 glow = b->c; glow.w = 0.16f;
    dr_rect(r2f32p(c.x - b->r*1.9f, c.y - b->r*1.9f, c.x + b->r*1.9f, c.y + b->r*1.9f), glow, b->r*1.9f, 0, b->r*0.9f);
    dr_rect(r2f32p(c.x - b->r, c.y - b->r, c.x + b->r, c.y + b->r), b->c, b->r, 0, 1.f);
    dr_rect(r2f32p(c.x - b->r*0.55f, c.y - b->r*0.7f, c.x - b->r*0.05f, c.y - b->r*0.2f), v4f32(1, 1, 1, 0.45f), b->r*0.25f, 0, 1.f);
  }
  for EachIndex(i, PARTICLE_MAX)
  {
    Particle *p = &app.parts[i];
    if(!p->alive) { continue; }
    Vec2F32 c = add_2f32(o, p->p);
    Vec4F32 col = p->c; col.w = fw_clamp01(p->life / p->max_life * 1.6f);
    dr_rect(r2f32p(c.x - p->size/2, c.y - p->size/2, c.x + p->size/2, c.y + p->size/2), col, p->size*0.2f, 0, 1.f);
  }
}
