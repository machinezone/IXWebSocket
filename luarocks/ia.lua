info()

batman = Player.new("Batman", 100)
robin  = Player.new("Robin", 100)

robin:say("I'm the team lea...")
batman:say("Shut up!\n")

print(batman:getName().." slap "..robin:getName())
math.randomseed(os.time())
robin:setHealth(math.random(20, 80))
robin:info()

print("\n")

print(batman:getName().." heal "..robin:getName())
batman:heal(robin)
robin:info()

-- Robin: I'm the team lea...
-- Batman: Shut up!
--
-- Batman slap Robin
-- Robin have 80 HP
--
-- Batman heal Robin
-- Robin have 100 HP