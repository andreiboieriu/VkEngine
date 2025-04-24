RotationSpeed = 60

---@return nil
function update()
    transform = getTransform()

    transform:Rotate(Vec3.new(0, 0, 1), RotationSpeed * -Time.deltaTime)
end
