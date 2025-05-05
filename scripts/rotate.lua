Speed = 0.7
RotationSpeed = 45

function init()
end

---@return nil
function update()
    transform = getTransform()
    transform:Rotate(Vec3.new(0, 1, 0), RotationSpeed * Time.deltaTime)
end
