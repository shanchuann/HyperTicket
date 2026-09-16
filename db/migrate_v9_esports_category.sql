-- v9: split esports events from the generic sports category.
UPDATE tickets
SET category = 'esports'
WHERE category = 'sports'
  AND (title LIKE '%英雄联盟%' OR title LIKE '%KPL%' OR title LIKE '%王者荣耀%'
       OR title LIKE '%电竞%' OR title LIKE '%全球总决赛%');
