RotationSpeed = 60

---@return nil
function update()
    transform = getTransform()

    transform.position = Vec3.new(0.2, 0.2, 1)
    transform:Rotate(Vec3.new(0, 0, 1), RotationSpeed * -Time.deltaTime)
end
