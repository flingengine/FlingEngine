totalTimeSinceSwitch = 0
movementOffset = vec3:new(5,0,0)
timeBetweenSwitch = 1.0

function start()
	Print("----- Lua start called ------")
end

function tick(deltaTime)
	-- totalTimeSinceSwitch = totalTimeSinceSwitch + deltaTime
	
	-- if totalTimeSinceSwitch > timeBetweenSwitch then
	-- 	totalTimeSinceSwitch = 0
	-- 	movementOffset.x = movementOffset.x * -1
	-- end
	
	-- currentPosition = entityTransform:GetPos()
	
	-- deltaX = movementOffset.x * deltaTime
	-- deltaY = movementOffset.y * deltaTime
	-- deltaZ = movementOffset.z * deltaTime
	
	
	-- currentPosition.x = currentPosition.x + deltaX
	-- currentPosition.y = currentPosition.y + deltaY
	-- currentPosition.z = currentPosition.z + deltaZ
	-- entityTransform:SetPos(currentPosition)
end